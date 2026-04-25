
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

#ifdef _WIN32
#include <Windows.h>
#include <winternl.h>
#else
#include "sys/epoll.h"
#include "sys/signalfd.h"
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

    if (standAlone)
    {
        RealBTarget::sortGraph();
        for (uint32_t i = 0; i < RealBTarget::sorted.size(); i++)
        {
            RealBTarget *rb = RealBTarget::sorted[i];
            rb->indexInTopologicalSort = i;
            rb->bTarget->generateStandAloneCommand();
        }
    }
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
    auto start = std::chrono::high_resolution_clock::now();
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

    uint32_t elementCount = 0;
    updateBTargets.clear();
    for (size_t i = RealBTarget::sorted.size(); i-- > 0;)
    {
        RealBTarget &localRb = *RealBTarget::sorted[i];
        if (!localRb.dependenciesSize)
        {
            updateBTargets.emplace_front(&localRb);
            localRb.insertionIndex = elementCount;
            ++elementCount;
        }
    }

    serverFd = createMultiplex();
    updateBTargetsSizeGoal = RealBTarget::sorted.size();

    // edit the following if you want to run in lesser threads.
    // cache.numberOfBuildThreads = cache.numberOfBuildThreads;
    const uint16_t numberOfLaunchedThreads = cache.numberOfBuildProcesses;
    idleCount = numberOfLaunchedThreads;
    maxSimultaneousProcessDesired = std::thread::hardware_concurrency() * 16;

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
    // 1. Block signals so they are consumed from signalfd.
    sigset_t mask;
    sigset_t oldMask;
    sigemptyset(&mask);
    if (sigaddset(&mask, SIGINT) == -1 || sigaddset(&mask, SIGTERM) == -1)
    {
        printErrorMessage(FORMAT("sigaddset failed. Error\n{}\n", P2978::getErrorString()));
    }
    if (sigprocmask(SIG_BLOCK, &mask, &oldMask) == -1)
    {
        printErrorMessage(FORMAT("sigprocmask(SIG_BLOCK) failed. Error\n{}\n", P2978::getErrorString()));
    }

    // 2. Create a signalfd for blocked signals.
    const int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd == -1)
    {
        printErrorMessage(FORMAT("signalfd failed. Error\n{}\n", P2978::getErrorString()));
    }

    // 3. Add signalfd to epoll.
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = sfd;
    if (epoll_ctl(serverFd, EPOLL_CTL_ADD, sfd, &ev) == -1)
    {
        printErrorMessage(FORMAT("epoll_ctl add signalfd failed. Error\n{}\n", P2978::getErrorString()));
    }

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
            const uint64_t pid = b->bTarget->run.pid;

            // Because it is gonna be a new process, we make sure that we don't exceed the process capacity, or we
            // do so only when we are down to 0 active process.
            const bool launchNewOne = pid == -1
                                          ? (maxSimultaneousProcessDesired > simultaneousProcessCount && idleCount) ||
                                                idleCount == numberOfLaunchedThreads
                                          : idleCount > 0;

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

            /*
            printMessage(getColorCode(ColorIndex::alice_blue));
            auto end = std::chrono::high_resolution_clock::now();
            printMessage(FORMAT("Active Event Count {} time-passed{}\n", activeEventCount, (end-start).count()));
            printMessage(getColorCode(ColorIndex::reset));
            */

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
                // +1 accounts for possible signalfd readiness event.
                if (n != -1 && n > activeEventCount + 1)
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
                const int fd = events[i].data.fd;
                if (fd == sfd)
                {
                    signalfd_siginfo signalInfo{};
                    const ssize_t bytesRead = read(sfd, &signalInfo, sizeof(signalInfo));
                    if (bytesRead == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            continue;
                        }
                        printErrorMessage(FORMAT("read(signalfd) failed. Error\n{}\n", P2978::getErrorString()));
                    }
                    if (bytesRead != sizeof(signalInfo))
                    {
                        printErrorMessage("Short read from signalfd\n");
                    }
                    string buffer;
                    writeBuildBuffer(buffer);
                    std::_Exit(EXIT_SUCCESS);
                }
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
#ifndef _WIN32
    if (epoll_ctl(serverFd, EPOLL_CTL_DEL, sfd, nullptr) == -1)
    {
        printErrorMessage(FORMAT("epoll_ctl del signalfd failed. Error\n{}\n", P2978::getErrorString()));
    }
    if (close(sfd) == -1)
    {
        printErrorMessage(FORMAT("close(signalfd) failed. Error\n{}\n", P2978::getErrorString()));
    }
    if (sigprocmask(SIG_SETMASK, &oldMask, nullptr) == -1)
    {
        printErrorMessage(FORMAT("sigprocmask(SIG_SETMASK) failed. Error\n{}\n", P2978::getErrorString()));
    }
#endif
}

uint64_t Builder::registerEventData(BTarget *target_, const uint64_t fd)
{
    ++activeEventCount;
#ifdef _WIN32
    if (unusedKeysIndices.empty())
    {
        const uint32_t index = currentIndex;
        ++currentIndex;

        auto &[overlappedBuffer, buffer, handle, target] = eventData[index];
        memset(overlappedBuffer, 0, sizeof(overlappedBuffer));
        handle = fd;
        target = target_;
        target->run.output = &buffer;
        // We need to provide a buffer where Windows kernel would write asynchronously.
        // On Linux,
        buffer.resize(4096);
        return index;
    }

    const uint32_t index = unusedKeysIndices.back();
    unusedKeysIndices.pop_back();

    auto &[overlappedBuffer, buffer, handle, target] = eventData[index];
    memset(overlappedBuffer, 0, sizeof(overlappedBuffer));
    handle = fd;
    target = target_;
    target->run.output = &buffer;
    buffer.clear();
    buffer.resize(4096);
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
            if constexpr (os == OS::NT)
            {
                unusedKeysIndices.emplace_back(index);
            }
            else
            {
                unusedOutputIndices.emplace_back(bTarget->run.outputIndex);
            }
            return false;
        }
    }
}

void Builder::unregisterEventDataAtIndex(const uint64_t index)
{
    --activeEventCount;
#ifndef _WIN32
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

    constexpr uint16_t configuredWorkers = 12;
    const uint16_t workerCount2 =
        static_cast<uint16_t>(std::min<size_t>(configuredWorkers, uncheckedNodesCentral.size()));
    const uint16_t workerCount =
        static_cast<uint16_t>(std::min<size_t>(workerCount2, std::thread::hardware_concurrency()));

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
        workers.emplace_back([chunk = chunks[i]] {
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
                uint32_t insertionIndex;
                updateBTargets.emplace(dependent, insertionIndex);
                dependent->insertionIndex = insertionIndex;
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