
#include "Builder.hpp"
#include "Cache.hpp"
#include "CppMod.hpp"
#include "JConsts.hpp"
#include "Manager.hpp"
#include "Node.hpp"
#include "PointerPacking.h"
#include "RunCommand.hpp"

#include <mutex>
#include <stack>
#include <thread>
#include <algorithm>

#ifdef _WIN32
#include <Windows.h>
#include <winternl.h>
#else
#include "sys/epoll.h"
#endif

using std::thread, std::mutex, std::make_unique, std::unique_ptr, std::ifstream, std::ofstream, std::stack,
    std::filesystem::current_path;

static Builder *consoleHandlerBuilder;
#ifdef _WIN32
BOOL WINAPI ConsoleHandler(DWORD signal)
{
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT)
    {
        if (!PostQueuedCompletionStatus((HANDLE)consoleHandlerBuilder->serverFd, 0, -1, NULL))
        {
            P2978::getErrorString("PostQueuedCompletionStatus");
        }
        return TRUE;
    }
}
#else
std::atomic<bool> signal_received;

void signal_handler(int signum)
{
    // Prevent re-entry if another signal arrives
    if (signal_received)
    {
        return;
    }
    signal_received = true;

    // Save progress
    string buffer;
    writeBuildBuffer(buffer);
    exit(EXIT_SUCCESS);
}

void registerSignalHandler()
{
    struct sigaction sa;
    sigset_t block_mask;

    // Initialize the sigaction structure
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;

    // Create a mask of signals to block during handler execution
    // This blocks ALL signals while the handler is running
    sigfillset(&block_mask);
    sa.sa_mask = block_mask;

    // SA_RESTART: Restart interrupted system calls
    sa.sa_flags = SA_RESTART;

    // Register handler for SIGINT (Ctrl+C)
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction SIGINT");
        exit(1);
    }

    // Register handler for SIGTERM (kill command)
    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("sigaction SIGTERM");
        exit(1);
    }
}
#endif

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
        printErrorMessage(P2978::getErrorString());
    }
    return reinterpret_cast<uint64_t>(iocp);
#else
    return epoll_create1(0);
#endif
}

Builder::Builder()
{
    round = 1;
    executeRoundOne();
    if (errorHappenedInRoundMode)
    {
        return;
    }

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        return;
    }
    checkNodes();
    delete[] BTarget::realBTargetsGlobal[1].data();
    --round;
    executeRoundZero();
}

void Builder::executeRoundOne()
{
    RealBTarget::graphEdges = span(BTarget::realBTargetsGlobal[round].data(), BTarget::realBTargetsArrayCount[round]);
    RealBTarget::sortGraph();

    for (RealBTarget *rb : RealBTarget::sorted)
    {
        if (!rb->dependenciesSize)
        {
            updateBTargets.emplace_back(rb);
        }
    }

    execute();
}

void Builder::executeRoundZero()
{
    RealBTarget::graphEdges = span(BTarget::realBTargetsGlobal[round].data(), BTarget::realBTargetsArrayCount[round]);
    RealBTarget::sortGraph();
    // RealBTarget::printSortedGraph();

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
                    if (bTargetDepType == BTargetDepType::FULL || bTargetDepType == BTargetDepType::SELECTIVE)
                    {
                        dependency->bTarget->selectiveBuild = true;
                    }
                }
            }
        }
    }

    updateBTargets.clear();
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

    // edit the following if you want to run in lesser threads.
   // cache.numberOfBuildThreads = cache.numberOfBuildThreads;
    const uint16_t numberOfLaunchedThreads = cache.numberOfBuildProcesses;
    idleCount = numberOfLaunchedThreads;
    maxSimultaneousProcessDesired = std::thread::hardware_concurrency() * 32;

    if (!idleCount)
    {
        printErrorMessage("number of maximum parallel process is 0\n");
    }

#ifdef _WIN32
    consoleHandlerBuilder = this;
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE))
    {
        P2978::getErrorString("SetConsoleCtrlHandler");
    }
#else
    // Linux system calls do not check for EINTR as we will not return from the interrupt handler.
    registerSignalHandler();
#endif

    while (true)
    {
        while (true)
        {
            const RealBTarget *b = updateBTargets.hasElement();
            if (!b)
            {
                break;
            }
            uint64_t pid = b->bTarget->run.pid;
            bool launchNewOne = false;
            if (pid == -1)
            {
                // Because it is gonna be a new process, we make sure that we don't exceed the process capacity, or we do
                // so only when we are down to 0 active process.

                const bool canLaunchNewProcess = maxSimultaneousProcessDesired > simultaneousProcessCount;
                launchNewOne = (canLaunchNewProcess && idleCount) || idleCount == numberOfLaunchedThreads;
            }
            else
            {
                if (idleCount)
                {
                    launchNewOne = true;
                }
            }

            if (!launchNewOne)
            {
                // printMessage(FORMAT("{}\n", simultaneousProcessCount));
                break;
            }

            updateBTargets.moveForward();
            ++updatedCount;

            if (b->bTarget->isEventRegistered(*this))
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
            if (!GetQueuedCompletionStatusEx((HANDLE)serverFd, events, 128, &n, INFINITE, FALSE))
            {
                printErrorMessage(P2978::getErrorString());
            }

            if constexpr (ndeb == NDEB::NO)
            {
                if (n > activeEventCount)
                {
                    printErrorMessage(FORMAT("n > activeCount, n {} activeCount {}\n", n, activeEventCount));
                }
            }
            for (ULONG i = 0; i < n; i++)
            {
                const uint64_t index = events[i].lpCompletionKey;
                if (index == -1)
                {
                    string buffer;
                    writeBuildBuffer(buffer);
                    exit(EXIT_SUCCESS);
                }
                CompletionKey &k = eventData[index];
                if constexpr (ndeb == NDEB::NO)
                {
                    if (&(OVERLAPPED &)k.overlappedBuffer != events[i].lpOverlapped)
                    {
                        // printErrorMessage("events[i].lpOverlapped != events[i].lpOverlapped\n");
                    }
                }

                if (BTarget *bTarget = k.target; bTarget && !callIsEventCompleted(bTarget, index))
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
                }
            }
            for (int i = 0; i < n; i++)
            {
                const uint64_t fd = events[i].data.fd;
                if (BTarget *bt = eventData[fd]; !callIsEventCompleted(bt, fd))
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

        if (idleCount == numberOfLaunchedThreads)
        {
            RealBTarget::sortGraph();
            printErrorMessage("HMake API misuse.\n");
        }
    }
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
        return index;
    }

    const uint32_t index = unusedKeysIndices.back();
    unusedKeysIndices.pop_back();

    // buffer can be reused as it was already initialized.
    auto &[overlappedBuffer, buffer, handle, target] = eventData[index];
    memset(overlappedBuffer, 0, sizeof(overlappedBuffer));
    handle = fd;
    target = target_;
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
                                 P2978::getErrorString()));
        HMAKE_HMAKE_INTERNAL_ERROR
    }
    return fd;
#endif
}

bool Builder::callIsEventCompleted(BTarget *bTarget, const uint64_t index)
{
    CompleteReadType completeReadType = bTarget->run.completeRead();
    if (completeReadType == CompleteReadType::INCOMPLETE)
    {
        if (!bTarget->run.startRead())
        {
            return true;
        }
    }

    while (true)
    {
        string message;
        if (completeReadType == CompleteReadType::COMPLETE_MESSAGE)
        {
            message = bTarget->run.pruneOutput();
        }
        else
        {
            unregisterEventDataAtIndex(index);
            bTarget->run.reapProcess(*this);
            bTarget->realBTargets[0].exitStatus = bTarget->run.exitStatus;
        }

        if (bTarget->isEventCompleted(*this, message))
        {
            if (!bTarget->run.startRead())
            {
                return true;
            }
        }
        else
        {
            return false;
        }
    }
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
                                 eventData[index]->getPrintName(), P2978::getErrorString()));
        HMAKE_HMAKE_INTERNAL_ERROR
    }
    eventData[index] = nullptr;
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

void Builder::checkNodes()
{
    uncheckedNodesCentral.clear();
    uncheckedNodesCentral.reserve(Node::idCount);
    for (uint32_t i = 0; i < Node::idCount; ++i)
    {
        if (nodeIndices[i]->toBeChecked)
        {
            uncheckedNodesCentral.emplace_back(nodeIndices[i]);
        }
    }

    if (uncheckedNodesCentral.empty())
    {
        return;
    }

    constexpr  uint16_t configuredWorkers = 12;
    const uint16_t workerCount2 = static_cast<uint16_t>(std::min<size_t>(configuredWorkers, uncheckedNodesCentral.size()));
    const uint16_t workerCount = static_cast<uint16_t>(std::min<size_t>(workerCount2, std::thread::hardware_concurrency()));

    if (workerCount == 1)
    {
        for (Node *node : uncheckedNodesCentral)
        {
            node->performSystemCheck();
        }
        return;
    }

    const vector<span<Node *>> chunks = divideInChunk(uncheckedNodesCentral, workerCount);
    vector<thread> workers;
    workers.reserve(workerCount - 1);

    for (uint16_t i = 1; i < workerCount; ++i)
    {
        workers.emplace_back([chunk = chunks[i]]() {
            for (Node *node : chunk)
            {
                node->performSystemCheck();
            }
        });
    }

    for (Node *node : chunks[0])
    {
        node->performSystemCheck();
    }

    for (thread &worker : workers)
    {
        worker.join();
    }
}

void Builder::execute()
{
    RealBTarget *rb = updateBTargets.getItem();

    while (rb)
    {
        if (round == 1)
        {
            rb->bTarget->setSelectiveBuild();
        }
        rb->bTarget->completeRoundOne();

        if (rb->exitStatus != EXIT_SUCCESS)
        {
            errorHappenedInRoundMode = true;
        }

        decrementFromDependents(*rb);
        rb = updateBTargets.getItem();
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

uint32_t Builder::getCapacityForNewProcesses() const
{
    if (const uint32_t desiredCapacity = maxSimultaneousProcessDesired - simultaneousProcessCount;
        desiredCapacity > idleCount)
    {
        return desiredCapacity;
    }
    return idleCount;
}