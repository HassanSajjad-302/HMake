
#include "Builder.hpp"
#include "Cache.hpp"
#include "Node.hpp"
#include <mutex>
#include <stack>
#include <thread>

using std::thread, std::mutex, std::make_unique, std::unique_ptr, std::ifstream, std::ofstream, std::stack,
    std::filesystem::current_path;

Builder::Builder()
{
    round = 1;
    RealBTarget::graphEdges =
        span(BTarget::realBTargetsGlobal[round].data(), BTarget::realBTargetsArrayCount[round].value);
    RealBTarget::sortGraph();

    for (RealBTarget *rb : RealBTarget::sorted)
    {
        if (!rb->dependenciesSize)
        {
            updateBTargets.emplace_back(rb);
        }
    }

    updateBTargetsSizeGoal = RealBTarget::sorted.size();

    vector<thread *> threads;

    launchedCount = cache.numberOfBuildThreads;

    if (launchedCount)
    {
        isOneThreadRunning = false;
        BTarget::laterDepsCentral.resize(launchedCount);
        threadIds.resize(launchedCount);

        while (threads.size() != launchedCount - 1)
        {
            uint64_t index = threads.size() + 1;
            threads.emplace_back(new thread([this, index] {
                BTarget::laterDepsCentral[index] = &BTarget::laterDepsLocal;
                threadIds[index] = getThreadId();
                myThreadIndex = index;
                execute();
            }));
        }
        execute();
    }
    else
    {
        printErrorMessage("maximumBuildThreads is zero\n");
        errorExit();
    }
    for (thread *t : threads)
    {
        t->join();
        delete t;
    }
}

// #define DEBUG_EXECUTE_YES
#ifdef DEBUG_EXECUTE_YES
#define DEBUG_EXECUTE(x) printMessage(x)
#else
#define DEBUG_EXECUTE(x)
#endif

extern string getThreadId();

#ifndef NDEBUG
unsigned short count = 0;
#endif

template <typename T> std::vector<std::span<T>> divideInChunk(std::vector<T> &v, uint16_t n)
{

    std::vector<std::span<T>> result;
    result.reserve(n);
    // If n is 1, return vector containing one span of the entire vector
    if (n == 1)
    {
        result.emplace_back(std::span<T>(v.data(), v.size()));
        return result;
    }

    // If n is greater than vector size, create n spans where first v.size() spans
    // contain one element each, and remaining spans are empty
    if (n > v.size())
    {

        // Create spans for existing elements (one element per span)
        for (size_t i = 0; i < v.size(); ++i)
        {
            result.emplace_back(v.data() + i, 1);
        }

        // Fill remaining spans as empty
        for (size_t i = v.size(); i < n; ++i)
        {
            result.emplace_back();
        }

        return result;
    }

    // Normal case: divide vector into n chunks
    size_t chunk_size = v.size() / n;
    size_t remainder = v.size() % n;
    size_t start_pos = 0;

    for (uint16_t i = 0; i < n; ++i)
    {
        // First 'remainder' chunks get an extra element
        size_t current_chunk_size = chunk_size + (i < remainder ? 1 : 0);

        result.emplace_back(v.data() + start_pos, current_chunk_size);
        start_pos += current_chunk_size;
    }

    return result;
}

void Builder::execute()
{
    RealBTarget *rb = nullptr;

    DEBUG_EXECUTE(FORMAT("{} Locking Update Mutex {} {} {}\n", round, __LINE__, sleepingCount, getThreadId()));
    std::unique_lock lk(executeMutex);
    while (true)
    {
        if (exeMode == ExecuteMode::WAIT)
        {
            if (rb = updateBTargets.getItem(); rb)
            {
                DEBUG_EXECUTE(FORMAT("{} update-executing {} {}\n", round, __LINE__, getThreadId()));
                DEBUG_EXECUTE(FORMAT("{} UnLocking Update Mutex {} {}\n", round, __LINE__, getThreadId()));
                const bool hasElement = updateBTargets.hasElement();
                executeMutex.unlock();
                if (hasElement)
                {
                    cond.notify_one();
                }
                if (round == 1)
                {
                    rb->bTarget->setSelectiveBuild();
                }
                bool isComplete = false;
                rb->bTarget->updateBTarget(*this, round, isComplete);
                if (isComplete)
                {
                    continue;
                }
                executeMutex.lock();

                if (rb->exitStatus != EXIT_SUCCESS)
                {
                    errorHappenedInRoundMode = true;
                }

                decrementFromDependents(rb);
                continue;
            }

            if (updateBTargets.size() == updateBTargetsSizeGoal && sleepingCount == launchedCount - 1)
            {
                isOneThreadRunning = true;
                if constexpr (bsMode == BSMode::BUILD)
                {
                    if (round && !errorHappenedInRoundMode)
                    {
                        uncheckedNodesCentral.reserve(Node::idCount);
                        for (uint32_t i = 0; i < Node::idCount; ++i)
                        {
                            if (nodeIndices[i]->toBeChecked)
                            {
                                uncheckedNodesCentral.emplace_back(nodeIndices[i]);
                            }
                        }
                        uncheckedNodes = divideInChunk(uncheckedNodesCentral, launchedCount);
                        exeMode = ExecuteMode::NODE_CHECK;
                        continue;
                    }
                }

                returnAfterWakeup = true;
                lk.release();
                executeMutex.unlock();
                cond.notify_one();
                DEBUG_EXECUTE(FORMAT("{} Locking after notifying one after round decrement {} {}\n", round, __LINE__,
                                     getThreadId()));
                DEBUG_EXECUTE(FORMAT("{} Returning after roundGoal Achieved{} {}\n", round, __LINE__, getThreadId()));
                return;
            }
        }
        else if (exeMode == ExecuteMode::PARALLEL)
        {
            // Maybe in build mode we do parallel round instead of wait round. As no CppTarget or LOAT depend on each
            // other in Build-Mode in round1. All load direct and transitive deps from cache. This would reduce the
            // number of executeMutex as just like node-check all thread will call updateBTarget in parallel instead of
            // waiting on each other.
            //
            // Should the node-check
            // round be floored to a thread-limit. Currently, it will use all threads
        }
        else if (exeMode == ExecuteMode::NODE_CHECK)
        {
            if (checkingCount < launchedCount)
            {
                const unsigned short nodeCheckIndex = checkingCount;
                ++checkingCount;
                DEBUG_EXECUTE(FORMAT("{} checking-count incremented {} {}\n", checkingCount, __LINE__, getThreadId()));
                executeMutex.unlock();
                cond.notify_one();
                for (Node *node : uncheckedNodes[nodeCheckIndex])
                {
                    node->performSystemCheck();
                    node->systemCheckCalled = true;
                    node->systemCheckCompleted = true;
                }
                DEBUG_EXECUTE(FORMAT("{} locking-mutex {} {}\n", checkedCount, __LINE__, getThreadId()));
                executeMutex.lock();
                ++checkedCount;
                DEBUG_EXECUTE(FORMAT("{} checked-count incremented {} {}\n", checkedCount, __LINE__, getThreadId()));
                continue;
            }

            if (checkedCount == launchedCount)
            {
                DEBUG_EXECUTE(
                    FORMAT("{} {} {}\n", round, "UPDATE_BTARGET threadCount == numberOfLaunchThreads", getThreadId()));

                BTarget::postRoundOneCompletion();
                --round;
                RealBTarget::graphEdges =
                    span(BTarget::realBTargetsGlobal[round].data(), BTarget::realBTargetsArrayCount[round].value);
                RealBTarget::sortGraph();
                // RealBTarget::printSortedGraph();

                updateBTargets.clear();
                updateBTargetsSizeGoal = 0;

                if (const size_t topSize = RealBTarget::sorted.size())
                {
                    for (size_t i = RealBTarget::sorted.size(); i-- > 0;)
                    {
                        RealBTarget &localRb = *RealBTarget::sorted[i];

                        localRb.indexInTopologicalSort = topSize - (i + 1);

                        if (localRb.bTarget->selectiveBuild)
                        {
                            for (auto &[dependency, bTargetDepType] : localRb.dependencies)
                            {
                                if (bTargetDepType == BTargetDepType::FULL ||
                                    bTargetDepType == BTargetDepType::SELECTIVE)
                                {
                                    dependency->bTarget->selectiveBuild = true;
                                }
                            }
                        }

                        if (!localRb.dependenciesSize)
                        {
                            updateBTargets.emplace_front(&localRb);
                        }
                    }
                }

                updateBTargetsSizeGoal = RealBTarget::sorted.size();
                isOneThreadRunning = false;
                exeMode = ExecuteMode::WAIT;
                continue;
            }
        }

        DEBUG_EXECUTE(FORMAT("{} Condition waiting {} {} {}\n", round, __LINE__, sleepingCount, getThreadId()));
        if (sleepingCount == launchedCount - 1)
        {
            RealBTarget::sortGraph();
            printErrorMessage("HMake API misuse.\n");
        }
        ++sleepingCount;
        cond.wait(lk);
        --sleepingCount;
        DEBUG_EXECUTE(
            FORMAT("{} Wakeup after condition waiting {} {} {} \n", round, __LINE__, sleepingCount, getThreadId()));
        if (returnAfterWakeup)
        {
            cond.notify_one();
            DEBUG_EXECUTE(
                FORMAT("{} returning after wakeup from condition variable {} {}\n", round, __LINE__, getThreadId()));
            return;
        }
    }
}

void Builder::decrementFromDependents(const RealBTarget *rb)
{
    DEBUG_EXECUTE(FORMAT("{} Locking in try block {} {}\n", round, __LINE__, getThreadId()));
    if (rb->exitStatus != EXIT_SUCCESS)
    {
        errorHappenedInRoundMode = true;
    }

    if (round)
    {
        for (auto &[dependent, bTargetDepType] : rb->dependents)
        {
            if (rb->exitStatus != EXIT_SUCCESS)
            {
                dependent->exitStatus = EXIT_FAILURE;
            }
            --dependent->dependenciesSize;
            if (!dependent->dependenciesSize)
            {
                updateBTargets.emplace(dependent);
            }
        }
    }
    else
    {
        for (auto &[dependent, bTargetDepType] : rb->dependents)
        {
            if (bTargetDepType == BTargetDepType::FULL)
            {
                if (rb->exitStatus != EXIT_SUCCESS)
                {
                    dependent->exitStatus = EXIT_FAILURE;
                }
                --dependent->dependenciesSize;
                if (!dependent->dependenciesSize)
                {
                    updateBTargets.emplace(dependent);
                }
            }
        }
    }

    DEBUG_EXECUTE(FORMAT("{} {} Info: updateBTargets.size() {} updateBTargetsSizeGoal {} {}\n", round, __LINE__,
                         updateBTargets.size(), updateBTargetsSizeGoal, getThreadId()));
}