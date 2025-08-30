
#include "Node.hpp"
#ifdef USE_HEADER_UNITS
import "Builder.hpp";
import "Settings.hpp";
import <mutex>;
import <stack>;
import <thread>;
#else
#include "Builder.hpp"
#include "Settings.hpp"
#include <mutex>
#include <stack>
#include <thread>
#endif
using std::thread, std::mutex, std::make_unique, std::unique_ptr, std::ifstream, std::ofstream, std::stack,
    std::filesystem::current_path;

Builder::Builder()
{
    round = 1;
    RealBTarget::graphEdges = span(BTarget::tarjanNodesBTargets[round].data(), BTarget::tarjanNodesCount[round]);
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

    numberOfLaunchedThreads = settings.maximumBuildThreads;
    if (numberOfLaunchedThreads)
    {
        for (uint64_t i = 0; i < numberOfLaunchedThreads - 1; ++i)
        {
            BTarget::laterDepsCentral.emplace_back(nullptr);
        }

        while (threads.size() != numberOfLaunchedThreads - 1)
        {
            uint64_t index = threads.size() + 1;
            threads.emplace_back(new thread([this, index] {
                BTarget::laterDepsCentral[index] = &BTarget::laterDepsLocal;
                myThreadId = index;
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
    const RealBTarget *rb = nullptr;

    DEBUG_EXECUTE(
        FORMAT("{} Locking Update Mutex {} {} {}\n", round, __LINE__, numberOfSleepingThreads.load(), getThreadId()));
    std::unique_lock lk(executeMutex);
    BTarget *last;
    while (true)
    {
        if (exeMode == ExecuteMode::GENERAL)
        {
            if (rb = updateBTargets.getItem(); rb)
            {
                DEBUG_EXECUTE(FORMAT("{} update-executing {} {}\n", round, __LINE__, getThreadId()));
                DEBUG_EXECUTE(FORMAT("{} UnLocking Update Mutex {} {}\n", round, __LINE__, getThreadId()));
                executeMutex.unlock();
                cond.notify_one();

                if (round == 1)
                {
                    rb->bTarget->setSelectiveBuild();
                }
                bool isComplete = false;
                last = rb->bTarget;
                rb->bTarget->updateBTarget(*this, round, isComplete);
                if (isComplete)
                {
                    continue;
                }
                executeMutex.lock();
                addNewTopBeUpdatedTargets(rb);

                continue;
            }

            if (updateBTargets.size() == updateBTargetsSizeGoal &&
                numberOfSleepingThreads == numberOfLaunchedThreads - 1)
            {
                if constexpr (bsMode == BSMode::BUILD)
                {
                    if (round && !errorHappenedInRoundMode)
                    {
                        uncheckedNodesCentral.reserve(Node::idCountCompleted);
                        for (uint32_t i = 0; i < Node::idCountCompleted; ++i)
                        {
                            if (Node::nodeIndices[i]->toBeChecked)
                            {
                                uncheckedNodesCentral.emplace_back(Node::nodeIndices[i]);
                            }
                        }
                        uncheckedNodes = divideInChunk(uncheckedNodesCentral, numberOfLaunchedThreads);
                        exeMode = ExecuteMode::NODE_CHECK;
                        executeMutex.unlock();
                        cond.notify_all();
                        executeMutex.lock();
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
        else if (exeMode == ExecuteMode::NODE_CHECK)
        {
            executeMutex.unlock();
            for (Node *node : uncheckedNodes[myThreadId])
            {
                node->performSystemCheck();
            }
            executeMutex.lock();
            if (numberOfSleepingThreads == numberOfLaunchedThreads - 1)
            {
                singleThreadRunning = true;
                DEBUG_EXECUTE(
                    FORMAT("{} {} {}\n", round, "UPDATE_BTARGET threadCount == numberOfLaunchThreads", getThreadId()));

                BTarget::runEndOfRoundTargets();
                --round;
                RealBTarget::graphEdges =
                    span(BTarget::tarjanNodesBTargets[round].data(), BTarget::tarjanNodesCount[round]);
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
                                if (bTargetDepType == BTargetDepType::FULL)
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
                exeMode = ExecuteMode::GENERAL;
                singleThreadRunning = false;
                continue;
            }
        }

        DEBUG_EXECUTE(
            FORMAT("{} Condition waiting {} {} {}\n", round, __LINE__, numberOfSleepingThreads.load(), getThreadId()));
        incrementNumberOfSleepingThreads();
        cond.wait(lk);
        decrementNumberOfSleepingThreads();
        DEBUG_EXECUTE(FORMAT("{} Wakeup after condition waiting {} {} {} \n", round, __LINE__,
                             numberOfSleepingThreads.load(), getThreadId()));
        if (returnAfterWakeup)
        {
            cond.notify_one();
            DEBUG_EXECUTE(
                FORMAT("{} returning after wakeup from condition variable {} {}\n", round, __LINE__, getThreadId()));
            return;
        }
    }
}

void Builder::addNewTopBeUpdatedTargets(const RealBTarget *rb)
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

void Builder::incrementNumberOfSleepingThreads()
{
    if (numberOfSleepingThreads == numberOfLaunchedThreads - 1)
    {
        RealBTarget::sortGraph();
        printErrorMessage("HMake API misuse.\n");
    }
    ++numberOfSleepingThreads;
}

void Builder::decrementNumberOfSleepingThreads()
{
    --numberOfSleepingThreads;
}