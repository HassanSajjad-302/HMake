
#include "Builder.hpp"
#include "Cache.hpp"
#include "Manager.hpp"
#include "Node.hpp"
#include "PointerPacking.h"
#include "RunCommand.hpp"

#include <mutex>
#include <stack>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#include <winternl.h>
#else
#include "sys/epoll.h"
#endif

using std::thread, std::mutex, std::make_unique, std::unique_ptr, std::ifstream, std::ofstream, std::stack,
    std::filesystem::current_path;

static uint64_t createMultiplex()
{
#ifdef _WIN32
    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, // handle to associate
                                         nullptr,              // existing IOCP handle
                                         0,                    // completion key (use pipe handle)
                                         0                     // number of concurrent threads (0 = default)
    );
    if (iocp == nullptr)
    {
        printErrorMessage(N2978::getErrorString());
    }
    return reinterpret_cast<uint64_t>(iocp);
#else
    return epoll_create1(0);
#endif
}

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

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        return;
    }

    for (size_t i = RealBTarget::sorted.size(); i-- > 0;)
    {
        RealBTarget &localRb = *RealBTarget::sorted[i];
        if (!localRb.dependenciesSize)
        {
            updateBTargets.emplace_front(&localRb);
        }
    }

    serverFd = createMultiplex();
    updateBTargetsSizeGoal = RealBTarget::sorted.size();

    uint64_t count = 0;
    idleCount = launchedCount;
    while (true)
    {
        while (idleCount)
        {
            const RealBTarget *b = updateBTargets.getItem();
            if (!b)
            {
                break;
            }
            ++updatedCount;

            if (b->bTarget->launchBTarget(*this))
            {
                --idleCount;
            }
            else
            {
                decrementFromDependents(*b);
            }
        }

        if (activeEventCount)
        {

#ifdef _WIN32
            OVERLAPPED_ENTRY events[128];
            ULONG n = 0;
            if (!GetQueuedCompletionStatusEx((HANDLE *)serverFd, events, 128, &n, INFINITE, FALSE))
            {
                printErrorMessage(N2978::getErrorString());
            }

            if constexpr (ndeb == NDEB::NO)
            {
                if (n > activeEventCount)
                {
                    printErrorMessage(FORMAT("n > activeCount, n {} activeCount {}\n", n, activeEventCount));
                }

                ++count;
                if (count == 5)
                {
                    bool breakpoint = true;
                }
            }
            for (ULONG i = 0; i < n; i++)
            {
                const uint64_t index = events[i].lpCompletionKey;
                CompletionKey &k = eventData[index];
                if constexpr (ndeb == NDEB::NO)
                {
                    if (&(OVERLAPPED &)k.overlappedBuffer != events[i].lpOverlapped)
                    {
                        // printErrorMessage("events[i].lpOverlapped != events[i].lpOverlapped\n");
                    }
                }
                if (BTarget *bTarget = k.target; bTarget && !bTarget->completeBTarget(*this, index, activeEventCount))
                {
                    decrementFromDependents(bTarget->realBTargets[0]);
                    ++idleCount;
                }
            }
#else
            epoll_event events[128];
            const int n = epoll_wait(serverFd, events, 128, -1);

            if constexpr (ndeb == NDEB::NO)
            {
                if (n > activeEventCount)
                {
                    for (uint32_t i = 0; i < 4096; i++)
                    {
                        if (eventData[i])
                        {
                            printMessage(eventData[i]->getPrintName() + '\n');
                        }
                    }
                    HMAKE_HMAKE_INTERNAL_ERROR
                    bool breakpoint = true;
                }
            }
            for (int i = 0; i < n; i++)
            {
                const uint64_t fd = events[i].data.fd;
                if (BTarget *bt = eventData[fd]; !bt->completeBTarget(*this, fd, activeEventCount))
                {
                    decrementFromDependents(bt->realBTargets[0]);
                    ++idleCount;
                }
            }
#endif
            continue;
        }

        if (updatedCount == updateBTargetsSizeGoal)
        {
            /*for (uint32_t i = 0; i < updateBTargetsSizeGoal; ++i)
            {
                printMessage(updateBTargets.array[i].value->bTarget->getPrintName() + '\n');
            }*/
            break;
        }
    }
    bool breakpoint = true;
    if (activeEventCount)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }
}

uint64_t Builder::registerEventData(BTarget *target_, const uint64_t fd)
{
    ++activeEventCount;
#ifdef _WIN32
    if (unusedKeysIndices.empty())
    {
        const uint32_t index = currentIndex;
        ++currentIndex;

        CompletionKey &k = eventData[index];
        memset(k.overlappedBuffer, 0, sizeof(k.overlappedBuffer));
        k.buffer = new char[4096]{};
        k.handle = fd;
        k.target = target_;
        k.firstCall = true;
        return index;
    }

    const uint32_t index = unusedKeysIndices.back();
    unusedKeysIndices.pop_back();

    // buffer can be reused as it was already initialized.
    auto &[overlappedBuffer, buffer, handle, target, firstCall] = eventData[index];
    memset(overlappedBuffer, 0, sizeof(overlappedBuffer));
    handle = fd;
    target = target_;
    firstCall = true;
    return index;
#else
    eventData[fd] = target_;
    epoll_event ev{};
    // Add stdout to epoll
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(serverFd, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        printErrorMessage(FORMAT("Failed to add BTarget\n{}\nread in epoll list. Error\n{}", target_->getPrintName(),
                                 N2978::getErrorString()));
        HMAKE_HMAKE_INTERNAL_ERROR
    }
    return fd;
#endif
}

void Builder::unregisterEventDataAtIndex(const uint64_t index)
{
    --activeEventCount;
#ifdef _WIN32
    unusedKeysIndices.emplace_back(index);
#else
    // Remove FDs from epoll now that process has exited
    if (epoll_ctl(serverFd, EPOLL_CTL_DEL, index, NULL) == -1)
    {
        printErrorMessage(FORMAT("Failed to remove BTarget\n{}\nread from epoll list. Error\n{}",
                                 eventData[index]->getPrintName(), N2978::getErrorString()));
        HMAKE_HMAKE_INTERNAL_ERROR
    }
    eventData[index] = nullptr;
#endif
}

#ifdef _WIN32
static bool readFromfd(CompletionKey &k)
{
    memset(k.overlappedBuffer, 0, sizeof(k.overlappedBuffer));
    DWORD bytesRead = 0;
    const BOOL result =
        ReadFile((HANDLE)k.handle, k.buffer, 4096, &bytesRead, &reinterpret_cast<OVERLAPPED &>(k.overlappedBuffer));
    if (result)
    {
        // output += string_view{k.buffer, bytesRead};
        // Event if synchronous succeeded, we need to process the stale completion event sent to the event loop.
        return false;
    }
    const DWORD error = GetLastError();
    if (error == ERROR_IO_PENDING)
    {
        return false;
    }
    if (error == ERROR_BROKEN_PIPE)
    {
        // ReadFile completed successfully
        return true;
    }

    printErrorMessage(N2978::getErrorString());
    return {};
}
#endif

ReadDataInfo Builder::isRecurrentReadFromEventDataCompleted(const uint64_t eventIndex, RunCommand &run)
{
#ifdef _WIN32
    CompletionKey &k = eventData[eventIndex];
    if (!k.firstCall)
    {
        uint64_t bytesRead = 0;
        if (!GetOverlappedResult((HANDLE)k.handle, &(OVERLAPPED &)k.overlappedBuffer, (LPDWORD)&bytesRead, false))
        {
            if (GetLastError() == ERROR_BROKEN_PIPE)
            {
                // ReadFile completed successfully
                return true;
            }
            printErrorMessage(N2978::getErrorString());
        }
        output += string_view{k.buffer, bytesRead};
        memset(k.buffer, 0, sizeof(k.buffer));
        if (output.ends_with(';'))
        {
            k.firstCall = true;
            return true;
        }
    }
    k.firstCall = false;
    return readFromfd(k);
#else
    char buffer[64 * 1024];
    const uint64_t readSize = read(eventIndex, buffer, sizeof(buffer) - 1);
    if (readSize == -1)
    {
        printErrorMessage(N2978::getErrorString());
    }
    if (!readSize)
    {
        return {"", true};
    }
    run.output.append(buffer, readSize);
    if (run.output.ends_with(N2978::delimiter))
    {
        return {run.pruneOutput(), true};
    }
    return {"", false};
#endif
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

                decrementFromDependents(*rb);
                continue;
            }

            if (updateBTargets.size() == updateBTargetsSizeGoal && idleCount == launchedCount - 1)
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
        // Should the node-check
        // round be floored to a thread-limit. Currently, it will use all threads
        // Another thought is make the performSystemCheck as multi-threaded and the whole build-system as
        // single-threaded.
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
                    }
                }

                updateBTargetsSizeGoal = 0;
                isOneThreadRunning = false;
                exeMode = ExecuteMode::WAIT;
                continue;
            }
        }

        DEBUG_EXECUTE(FORMAT("{} Condition waiting {} {} {}\n", round, __LINE__, sleepingCount, getThreadId()));
        if (idleCount == launchedCount - 1)
        {
            RealBTarget::sortGraph();
            printErrorMessage("HMake API misuse.\n");
        }
        ++idleCount;
        cond.wait(lk);
        --idleCount;
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

void Builder::decrementFromDependents(const RealBTarget &rb)
{
    DEBUG_EXECUTE(FORMAT("{} Locking in try block {} {}\n", round, __LINE__, getThreadId()));
    if (rb.exitStatus != EXIT_SUCCESS)
    {
        errorHappenedInRoundMode = true;
    }

    for (auto &[dependent, bTargetDepType] : rb.dependents)
    {
        if (bTargetDepType == BTargetDepType::FULL)
        {
            if (rb.exitStatus != EXIT_SUCCESS)
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

    DEBUG_EXECUTE(FORMAT("{} {} Info: updateBTargets.size() {} updateBTargetsSizeGoal {} {}\n", round, __LINE__,
                         updateBTargets.size(), updateBTargetsSizeGoal, getThreadId()));
}