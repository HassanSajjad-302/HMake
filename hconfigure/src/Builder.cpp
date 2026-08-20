
#include "Builder.hpp"
#include "Cache.hpp"
#include "JConsts.hpp"
#include "Manager.hpp"
#include "Node.hpp"
#include "RunCommand.hpp"

#include <cerrno>
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

/// Builder uses two complementary scheduling models. Round 1 is a synchronous graph walk that lets targets finish
/// configuration and establish a stable dependency graph. Round 0 is an asynchronous process scheduler: a target is
/// placed on `readyBTargets` only when all of its FULL predecessors have completed, then either finishes immediately
/// or registers an event whose completion releases its dependents.
///
/// In other words, `readyBTargets` is a readiness frontier rather than a generic work list. `dependenciesSize`,
/// `updatedCount`, and `availableProcessSlots` respectively track graph readiness, completion accounting, and process
/// pressure; together they guarantee that the frontier advances without oversubscribing the machine.
static Builder *consoleHandlerBuilder;
#ifdef _WIN32
BOOL WINAPI ConsoleHandler(DWORD signal)
{
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT)
    {
        if (consoleHandlerBuilder &&
            !PostQueuedCompletionStatus((HANDLE)consoleHandlerBuilder->serverFd, 0, static_cast<ULONG_PTR>(-1), NULL))
        {
            P2978::getErrorString("PostQueuedCompletionStatus");
        }
        return TRUE;
    }
    return FALSE;
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
        printErrorMessage(FORMAT("Could not create the Windows build event loop.\nOperation: CreateIoCompletionPort\n"
                                 "System error: {}",
                                 P2978::getErrorString()));
    }
    return reinterpret_cast<uint64_t>(iocp);
#else
    const int epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (epollFd == -1)
    {
        printErrorMessage(FORMAT("Could not create the Linux build event loop.\nOperation: epoll_create1\n"
                                 "System error: {}",
                                 P2978::getErrorString()));
    }
    return static_cast<uint64_t>(epollFd);
#endif
}

Builder::Builder()
{
    // Finish round-one graph construction before the general filesystem snapshot. Adaptive source metadata was
    // already refreshed during post-configuration so round one can partition by current file size.
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
    // The graph now knows the complete initial node set, so snapshot its relevant file state in parallel.
    checkNodes();
    delete[] BTarget::realBTargetsGlobal[1].data();
    BTarget::realBTargetsGlobal[1] = {};
    --round;
    executeRoundZero();

    if (standAlone)
    {
        RealBTarget::sortGraph();
        for (uint32_t i = 0; i < RealBTarget::sorted.size(); i++)
        {
            RealBTarget *rb = RealBTarget::sorted[i];
            rb->indexInTopologicalSort = i;
            rb->getBTarget()->generateStandAloneCommand();
        }
    }
}

void Builder::executeRoundOne()
{
    // Seed the readiness frontier from a topological order. `execute()` extends it synchronously as nodes complete.
    RealBTarget::graphEdges = span(BTarget::realBTargetsGlobal[round].data(), BTarget::realBTargetsArrayCount[round]);
    RealBTarget::sortGraph();
    readyBTargetsSizeGoal = RealBTarget::sorted.size();
    updatedCount = 0;
    readyBTargets.reserve(RealBTarget::sorted.size());

    for (RealBTarget *rb : RealBTarget::sorted)
    {
        if (!rb->dependenciesSize)
        {
            readyBTargets.emplace_back(rb);
        }
    }

    execute();
    if (updatedCount != readyBTargetsSizeGoal)
    {
        // Unreachable for a closed, immutable, acyclic graph whose FULL/WAIT edge counts are symmetric. Retain the
        // check as a cheap guard against future custom targets mutating raw scheduler state during round one.
        printErrorMessage(FORMAT("Internal round-one scheduler invariant failed.\nCompleted targets: {}\n"
                                 "Expected targets: {}\n"
                                 "Hint: a target, blocking edge, dependency count, or queue entry changed during "
                                 "round-one execution.",
                                 updatedCount, readyBTargetsSizeGoal));
    }
}

void Builder::executeRoundZero()
{
    auto start = std::chrono::high_resolution_clock::now();
    RealBTarget::graphEdges = span(BTarget::realBTargetsGlobal[round].data(), BTarget::realBTargetsArrayCount[round]);
    RealBTarget::sortGraph();
    // RealBTarget::printSortedGraph();

    if (const size_t topologicalTargetCount = RealBTarget::sorted.size())
    {
        // Visit consumers before producers: selective work pulls its required dependencies into the build, while a
        // changed relationship updates the dependency contract persisted for the consumer.
        for (size_t reverseTopologicalIndex = RealBTarget::sorted.size(); reverseTopologicalIndex-- > 0;)
        {
            RealBTarget &target = *RealBTarget::sorted[reverseTopologicalIndex];

            target.indexInTopologicalSort = topologicalTargetCount - (reverseTopologicalIndex + 1);

            if (target.getBTarget()->selectiveBuild)
            {
                // The cached dependencies may exist even when the target has no current dependencies.
                if (target.checkDepsChanged())
                {
                    const uint32_t cacheIndex = target.getBTarget()->cacheIndex;
                    BTargetCache &targetCache = bTargetCaches[cacheIndex];

                    string &updatedDependencies = *new string();
                    updatedDependencies.reserve(sizeof(uint32_t) + sizeof(uint32_t) * target.dependenciesSize);
                    writeUint32(updatedDependencies, target.dependenciesSize);
                    uint32_t observedBlockingDependencies = 0;
                    target.updateStatus = UpdateStatus::UPDATE_NEEDED;

                    for (const RBTWithType &dependency : target.dependencies)
                    {
                        const RelationType relationType = dependency.getRelationType();
                        BTarget *dependencyTarget = dependency.getPointer()->getBTarget();

                        if (relationType == RelationType::FULL || relationType == RelationType::SELECTIVE)
                        {
                            dependencyTarget->selectiveBuild = true;
                        }

                        if (isBlockingRelation(relationType))
                        {
                            writeUint32(updatedDependencies, dependencyTarget->cacheIndex);
                            ++observedBlockingDependencies;
                        }
                    }
                    if (observedBlockingDependencies != target.dependenciesSize)
                    {
                        printErrorMessage(FORMAT("Internal dependency-count invariant failed.\nTarget: {}\n"
                                                 "Recorded blocking count: {}\nObserved blocking dependencies: {}",
                                                 target.getBTarget()->getPrintName(),
                                                 static_cast<uint32_t>(target.dependenciesSize),
                                                 observedBlockingDependencies));
                    }
                    targetCache.depsCache = updatedDependencies;
                }
                else
                {
                    for (const RBTWithType &dependency : target.dependencies)
                    {
                        if (dependency.getRelationType() == RelationType::FULL ||
                            dependency.getRelationType() == RelationType::SELECTIVE)
                        {
                            dependency.getPointer()->getBTarget()->selectiveBuild = true;
                        }
                    }
                }
            }
        }
    }

    // Recreate the readiness frontier from the final graph. `insertionIndex` allows a module with blocked consumers to
    // be promoted later, reducing the number of idle compiler processes.
    uint32_t readyTargetCount = 0;
    readyBTargets.clear();
    // Most targets consume one queue slot. Dynamic module promotions may add tombstoned replacement slots later,
    // for which PointerArrayList retains geometric growth.
    readyBTargets.reserve(RealBTarget::sorted.size());
    for (size_t reverseTopologicalIndex = RealBTarget::sorted.size(); reverseTopologicalIndex-- > 0;)
    {
        RealBTarget &target = *RealBTarget::sorted[reverseTopologicalIndex];
        if (!target.dependenciesSize)
        {
            readyBTargets.emplace_front(&target);
            target.insertionIndex = readyTargetCount;
            ++readyTargetCount;
        }
    }

    serverFd = createMultiplex();
    readyBTargetsSizeGoal = RealBTarget::sorted.size();
    updatedCount = 0;

    // One limit caps active child processes; the other limits aggregate compiler pressure. A fully idle scheduler may
    // still start one process, so conservative throttling cannot prevent the graph from making initial progress.
    const uint16_t maxRunningProcessAllowed = cache.numberOfBuildProcesses;
    availableProcessSlots = maxRunningProcessAllowed;

    if (!availableProcessSlots)
    {
        printErrorMessage("Invalid process limit.\nConfigured parallel-process count: 0\nHint: set it to at least 1.");
    }
    const uint32_t hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
    maxSimultaneousProcessDesired = hardwareThreads * 8;

#ifdef _WIN32
    consoleHandlerBuilder = this;
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE))
    {
        P2978::getErrorString("SetConsoleCtrlHandler");
    }
#else
    // Feed cancellation through the same event loop as child output. Blocking first prevents asynchronous delivery
    // before signalfd can observe the signal.
    sigset_t mask;
    sigset_t oldMask;
    sigemptyset(&mask);
    if (sigaddset(&mask, SIGINT) == -1 || sigaddset(&mask, SIGTERM) == -1)
    {
        printErrorMessage(
            FORMAT("Could not add SIGINT/SIGTERM to the build signal set.\nSystem error: {}", P2978::getErrorString()));
    }
    if (sigprocmask(SIG_BLOCK, &mask, &oldMask) == -1)
    {
        printErrorMessage(FORMAT("Could not block build termination signals.\nOperation: sigprocmask(SIG_BLOCK)\n"
                                 "System error: {}",
                                 P2978::getErrorString()));
    }

    const int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd == -1)
    {
        printErrorMessage(FORMAT("Could not create the build signal file descriptor.\nOperation: signalfd\n"
                                 "System error: {}",
                                 P2978::getErrorString()));
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = sfd;
    if (epoll_ctl(serverFd, EPOLL_CTL_ADD, sfd, &ev) == -1)
    {
        printErrorMessage(FORMAT("Could not register the signal file descriptor with the event loop.\n"
                                 "Operation: epoll_ctl(EPOLL_CTL_ADD)\nFile descriptor: {}\nSystem error: {}",
                                 sfd, P2978::getErrorString()));
    }

#endif

    while (true)
    {
        while (true)
        {
            const RealBTarget *next = readyBTargets.hasElement();
            if (!next)
            {
                break;
            }
            BTarget *const target = next->getBTarget();
            const uint64_t pid = target->run.pid;

            bool launchNewOne;
            if (pid != -1)
            {
                // Resuming an existing process does not add process pressure.
                launchNewOne = availableProcessSlots > 0;
            }
            else
            {
                const bool hasSlot = availableProcessSlots > 0;
                const bool underLimit = simultaneousProcessCount < maxSimultaneousProcessDesired;
                const bool nothingRunning = availableProcessSlots == maxRunningProcessAllowed;
                launchNewOne = hasSlot && (underLimit || nothingRunning);
            }

            if (!launchNewOne)
            {
                // printMessage(FORMAT("{}\n", simultaneousProcessCount));
                break;
            }

            readyBTargets.moveForward();

            if (!target->initiationTime)
            {
                target->initiationTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                             std::chrono::system_clock::now().time_since_epoch())
                                             .count();
            }

            if (target->isEventRegistered(*this))
            {
                if (!availableProcessSlots)
                {
                    printErrorMessage(FORMAT("Internal process-slot underflow before target launch.\nTarget: {}\n"
                                             "Configured slots: {}",
                                             target->getPrintName(), maxRunningProcessAllowed));
                }
                --availableProcessSlots;
            }
            else
            {
                decrementFromDependents(const_cast<RealBTarget &>(*next));
            }
        }

        if (availableProcessSlots == maxRunningProcessAllowed)
        {
            break;
        }

        /*
        printMessage(getColorCode(ColorIndex::alice_blue));
        auto end = std::chrono::high_resolution_clock::now();
        printMessage(FORMAT("Active Event Count {} time-passed{}\n", activeEventCount, (end-start).count()));
        printMessage(getColorCode(ColorIndex::reset));
        */

#ifdef _WIN32
        OVERLAPPED_ENTRY completionEvents[128];
        ULONG completionEventCount = 0;
        if (!GetQueuedCompletionStatusEx((HANDLE)serverFd, completionEvents, 128, &completionEventCount, INFINITE,
                                         FALSE))
        {
            printErrorMessage(FORMAT("Could not read events from the Windows build event loop.\n"
                                     "Operation: GetQueuedCompletionStatusEx\nSystem error: {}",
                                     P2978::getErrorString()));
        }

        for (ULONG completionEventIndex = 0; completionEventIndex < completionEventCount; completionEventIndex++)
        {
            const uint64_t eventIndex = completionEvents[completionEventIndex].lpCompletionKey;
            if (eventIndex == -1)
            {
                const string buildCache = getBuildCache();
                writeNodesCache();
                // getBuildCache() deliberately returns an empty string when no completed target changed the cache.
                // Preserve the previous cache in that case. Replacing it with an empty file makes the next
                // configure/build invocation interpret missing records as a serialized build cache.
                if (!buildCache.empty())
                {
                    writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("build-cache"),
                                                buildCache);
                }
                std::_Exit(EXIT_SUCCESS);
            }
            if (eventIndex >= currentIndex)
            {
                printErrorMessage(FORMAT("Windows build event has an invalid completion key.\n"
                                         "Completion key: {}\nAllocated keys: {}",
                                         eventIndex, currentIndex));
            }
            CompletionKey &completionKey = eventData[eventIndex];
            if constexpr (ndeb == NDEB::NO)
            {
                if (&(OVERLAPPED &)completionKey.overlappedBuffer !=
                    completionEvents[completionEventIndex].lpOverlapped)
                {
                    // printErrorMessage("completion event does not match its completion key\n");
                }
            }

            if (BTarget *target = completionKey.target; target)
            {
                if (!callIsEventCompleted(target, eventIndex))
                {
                    decrementFromDependents(target->realBTargets[0]);
                    if (availableProcessSlots >= maxRunningProcessAllowed)
                    {
                        printErrorMessage(FORMAT("Internal process-slot invariant failed after target completion.\n"
                                                 "Target: {}\nAvailable slots: {}\nConfigured slots: {}",
                                                 target->getPrintName(), availableProcessSlots,
                                                 maxRunningProcessAllowed));
                    }
                    ++availableProcessSlots;
                }
            }
        }
#else
        epoll_event readyEvents[128];
        const int readyEventCount = epoll_wait(serverFd, readyEvents, 128, -1);
        if (readyEventCount == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            printErrorMessage(FORMAT("Could not read events from the Linux build event loop.\n"
                                     "Operation: epoll_wait\nSystem error: {}",
                                     P2978::getErrorString()));
        }

        if constexpr (ndeb == NDEB::NO)
        {
            // +1 accounts for possible signalfd readiness event.
            if (readyEventCount != -1 && readyEventCount > maxRunningProcessAllowed - availableProcessSlots + 1)
            {
                for (const BTarget *ptr : eventData)
                {
                    if (ptr)
                    {
                        printMessage(ptr->getPrintName() + '\n');
                    }
                }
                HMAKE_HMAKE_INTERNAL_ERROR
            }
        }

        for (int readyEventIndex = 0; readyEventIndex < readyEventCount; readyEventIndex++)
        {
            const int eventFd = readyEvents[readyEventIndex].data.fd;
            if (eventFd == sfd)
            {
                signalfd_siginfo signalInfo{};
                const ssize_t bytesRead = read(sfd, &signalInfo, sizeof(signalInfo));
                if (bytesRead == -1)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        continue;
                    }
                    printErrorMessage(FORMAT("Could not read a build termination signal.\nFile descriptor: {}\n"
                                             "Operation: read(signalfd)\nSystem error: {}",
                                             sfd, P2978::getErrorString()));
                }
                if (bytesRead != sizeof(signalInfo))
                {
                    printErrorMessage(FORMAT("Build termination signal was truncated.\nFile descriptor: {}\n"
                                             "Expected bytes: {}\nRead bytes: {}",
                                             sfd, sizeof(signalInfo), bytesRead));
                }

                const string buildCache = getBuildCache();
                writeNodesCache();
                // getBuildCache() deliberately returns an empty string when no completed target changed the cache.
                // Preserve the previous cache in that case. Replacing it with an empty file makes the next
                // configure/build invocation interpret missing records as a serialized build cache.
                if (!buildCache.empty())
                {
                    writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("build-cache"),
                                                buildCache);
                }
                std::_Exit(EXIT_SUCCESS);
            }
            if (eventFd < 0 || static_cast<size_t>(eventFd) >= eventData.size())
            {
                printErrorMessage(FORMAT("Linux build event has an invalid file descriptor.\n"
                                         "File descriptor: {}\nEvent table size: {}",
                                         eventFd, eventData.size()));
            }
            BTarget *bt = eventData[eventFd];
            if (!bt)
            {
                printErrorMessage(FORMAT("Linux build event has no target.\nFile descriptor: {}", eventFd));
            }
            if (!callIsEventCompleted(bt, eventFd))
            {
                decrementFromDependents(bt->realBTargets[0]);
                if (availableProcessSlots >= maxRunningProcessAllowed)
                {
                    printErrorMessage(FORMAT("Internal process-slot invariant failed after target completion.\n"
                                             "Target: {}\nAvailable slots: {}\nConfigured slots: {}",
                                             bt->getPrintName(), availableProcessSlots, maxRunningProcessAllowed));
                }
                ++availableProcessSlots;
            }
        }
#endif
    }

    if (updatedCount != readyBTargetsSizeGoal)
    {
        // At this point the list must be empty
        if (readyBTargets.hasElement())
        {
            HMAKE_HMAKE_INTERNAL_ERROR
        }
        /*for (uint32_t i = 0; i < readyBTargetsSizeGoal; ++i)
        {
            printMessage(readyBTargets.array[i].value->bTarget->getPrintName() + '\n');
        }*/
        RealBTarget::sortGraph();

        string blockedTargets;
        uint32_t reported = 0;
        for (const RealBTarget *target : RealBTarget::sorted)
        {
            if (target->isCompleted)
            {
                continue;
            }
            blockedTargets += FORMAT("\n  - {} (remaining blocking dependencies: {})",
                                     target->getBTarget()->getPrintName(), target->dependenciesSize);
            if (++reported == 12)
            {
                break;
            }
        }
        printErrorMessage(FORMAT("Build graph could not make progress.\nCompleted targets: {}\nExpected targets: {}\n"
                                 "Blocked targets:{}\nHint: inspect FULL/WAIT dependencies and target process state.",
                                 updatedCount, readyBTargetsSizeGoal,
                                 blockedTargets.empty() ? "\n  - <none reported>" : blockedTargets));
    }

#ifndef _WIN32
    if (epoll_ctl(serverFd, EPOLL_CTL_DEL, sfd, nullptr) == -1)
    {
        printErrorMessage(FORMAT("Could not unregister the signal descriptor from the event loop.\n"
                                 "File descriptor: {}\nOperation: epoll_ctl(EPOLL_CTL_DEL)\nSystem error: {}",
                                 sfd, P2978::getErrorString()));
    }
    if (close(sfd) == -1)
    {
        printErrorMessage(FORMAT("Could not close the signal descriptor.\nFile descriptor: {}\nSystem error: {}", sfd,
                                 P2978::getErrorString()));
    }
    if (sigprocmask(SIG_SETMASK, &oldMask, nullptr) == -1)
    {
        printErrorMessage(FORMAT("Could not restore the process signal mask.\nOperation: sigprocmask(SIG_SETMASK)\n"
                                 "System error: {}",
                                 P2978::getErrorString()));
    }
    if (close(static_cast<int>(serverFd)) == -1)
    {
        printErrorMessage(FORMAT("Could not close the Linux build event loop.\nFile descriptor: {}\nSystem error: {}",
                                 serverFd, P2978::getErrorString()));
    }
#else
    if (!SetConsoleCtrlHandler(ConsoleHandler, FALSE))
    {
        printErrorMessage(
            FORMAT("Could not unregister the Windows console handler.\nSystem error: {}", P2978::getErrorString()));
    }
    consoleHandlerBuilder = nullptr;
    if (!CloseHandle((HANDLE)serverFd))
    {
        printErrorMessage(
            FORMAT("Could not close the Windows build event loop.\nSystem error: {}", P2978::getErrorString()));
    }
#endif
    serverFd = static_cast<uint64_t>(-1);
}

uint64_t Builder::registerEventData(BTarget *target_, const uint64_t fd)
{
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
        // IOCP needs storage that survives until its asynchronous completion. Linux keeps only an fd-to-target map;
        // RunCommand owns the associated read buffer there.
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
    if (fd >= eventData.size())
    {
        eventData.resize(std::max<size_t>(fd + 1, eventData.size() * 2), nullptr);
    }
    eventData[fd] = target_;
    epoll_event ev{};
    // Add stdout to epoll
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(serverFd, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        printErrorMessage(FORMAT("Could not register target output with the build event loop.\nTarget: {}\n"
                                 "File descriptor: {}\nOperation: epoll_ctl(EPOLL_CTL_ADD)\nSystem error: {}",
                                 target_->getPrintName(), fd, P2978::getErrorString()));
        HMAKE_HMAKE_INTERNAL_ERROR
    }
    return fd;
#endif
}

bool Builder::callIsEventCompleted(BTarget *bTarget, const uint64_t index)
{
    // Readability is not synonymous with process completion: IPC module builds can produce multiple messages. Keep
    // dispatching until the target needs another read or explicitly completes; only completion releases dependents.
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
        // A valid framed payload is nonempty. The empty value is reserved for COMPLETE_PROCESS after the child has
        // been reaped; scheduler wakeups must use their own callback path rather than impersonating process EOF.
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
            // Windows may observe EOF synchronously while starting the next overlapped read.
            completeReadType = CompleteReadType::COMPLETE_PROCESS;
        }
        else
        {
#ifdef _WIN32
            unusedKeysIndices.emplace_back(index);
#else
            freeOutputStrings.push_back(bTarget->run.output);
#endif
            return false;
        }
    }
}

void Builder::unregisterEventDataAtIndex(const uint64_t index)
{
#ifndef _WIN32
    // The child has been reaped before this point, so this descriptor can no longer produce useful scheduler events.
    if (epoll_ctl(serverFd, EPOLL_CTL_DEL, index, NULL) == -1)
    {
        printErrorMessage(FORMAT("Could not unregister target output from the build event loop.\nTarget: {}\n"
                                 "File descriptor: {}\nOperation: epoll_ctl(EPOLL_CTL_DEL)\nSystem error: {}",
                                 eventData[index]->getPrintName(), index, P2978::getErrorString()));
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

template <typename T> void divideInChunk(vector<std::span<T>> &result, vector<T> &v, uint16_t n)
{
    // Produce non-owning partitions for regular-cost work. Content hashing uses a different strategy below because
    // file sizes make its individual items highly uneven.
    if (n == 1)
    {
        result.emplace_back(std::span<T>(v.data(), v.size()));
        return;
    }

    if (n > v.size())
    {
        for (size_t i = 0; i < v.size(); ++i)
        {
            result.emplace_back(v.data() + i, 1);
        }

        for (size_t i = v.size(); i < n; ++i)
        {
            result.emplace_back();
        }

        return;
    }

    const size_t chunk_size = v.size() / n;
    const size_t remainder = v.size() % n;
    size_t start_pos = 0;

    for (uint16_t i = 0; i < n; ++i)
    {
        size_t current_chunk_size = chunk_size + (i < remainder ? 1 : 0);

        result.emplace_back(v.data() + start_pos, current_chunk_size);
        start_pos += current_chunk_size;
    }
}

void Builder::checkNodes()
{
    vector<Node *> statNodes;
    vector<Node *> hashNodes;
    statNodes.reserve(Node::idCount);
    hashNodes.reserve(Node::idCount);

    // statCompleted/hashCompleted distinguish the initial snapshot from later calls. The same pass therefore also
    // picks up headers and other nodes discovered while the build is running.
    for (uint32_t i = 0; i < Node::idCount; ++i)
    {
        Node *node = nodeIndices[i];
        if (!node->statCompleted && (node->doStatFile || node->doHashFile))
        {
            statNodes.emplace_back(node);
        }
        if (node->doHashFile && !node->hashCompleted)
        {
            hashNodes.emplace_back(node);
        }
    }

    const uint32_t hwc = [] {
        const uint32_t n = std::thread::hardware_concurrency();
        return n ? n : 1;
    }();

    // Stat work is cheap and regular, so contiguous static chunks minimize coordination overhead. Some hash nodes,
    // notably adaptive-unity sources, were already stat'ed before round one and bypass this block.
    if (!statNodes.empty())
    {
        const uint32_t workerCount = std::min<uint32_t>(hwc, statNodes.size());
        vector<std::span<Node *>> chunks;
        chunks.reserve(workerCount);
        divideInChunk(chunks, statNodes, workerCount);

        vector<thread> workers;
        workers.reserve(workerCount - 1);
        for (uint32_t i = 1; i < workerCount; ++i)
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
        for (thread &w : workers)
        {
            w.join();
        }
    }

    // Hashing cost is size-sensitive. Sorting largest-first and assigning by stride approximates longest-processing-
    // time scheduling, keeping a few large files from leaving one worker busy after the others finish.
    if (hashNodes.empty())
    {
        return;
    }

    // performSystemCheck() resolves unchanged regular files from the persisted hash. Missing files use their sentinel
    // hash. Remove both in-place before partitioning the remaining hashing work.
    uint32_t validCount = static_cast<uint32_t>(hashNodes.size());
    for (uint32_t i = 0; i < validCount;)
    {
        Node *node = hashNodes[i];
        if (node->fileType == file_type::not_found)
        {
            node->contentHash = Node::missingContentHash;
            node->hashCompleted = true;
        }

        if (node->hashCompleted)
        {
            std::swap(hashNodes[i], hashNodes[--validCount]);
            // i stays: the swapped-in element must be rechecked.
        }
        else
        {
            ++i;
        }
    }

    hashNodes.resize(validCount);

    if (hashNodes.empty())
    {
        return;
    }

    const uint32_t workerCount = std::min<uint32_t>(hwc, static_cast<uint32_t>(hashNodes.size()));

    std::ranges::sort(hashNodes, [](const Node *a, const Node *b) { return a->fileSize > b->fileSize; });

    const auto hashStride = [&](const uint32_t threadId) {
        for (uint32_t i = threadId; i < static_cast<uint32_t>(hashNodes.size()); i += workerCount)
        {
            hashNodes[i]->performContentHash();
        }
    };

    vector<thread> workers;
    workers.reserve(workerCount - 1);
    for (uint32_t i = 1; i < workerCount; ++i)
    {
        workers.emplace_back([i, &hashStride] { hashStride(i); });
    }
    hashStride(0);
    for (thread &w : workers)
    {
        w.join();
    }
}

void Builder::execute()
{
    // Round 1 has no child processes, so every completion immediately advances the readiness frontier.
    RealBTarget *rb = readyBTargets.getItem();

    while (rb)
    {
        if (round == 1)
        {
            rb->getBTarget()->setSelectiveBuild();
        }
        rb->getBTarget()->completeRoundOne();

        if (rb->exitStatus != EXIT_SUCCESS)
        {
            errorHappenedInRoundMode = true;
        }

        decrementFromDependents(*rb);
        rb = readyBTargets.getItem();
    }
}

void Builder::decrementFromDependents(RealBTarget &rb)
{
    // This is the graph's commit point: propagate rebuild/failure state and one predecessor completion to each FULL
    // consumer. A consumer becomes runnable exactly when its final prerequisite commits here.
    if (rb.isCompleted)
    {
        printErrorMessage(FORMAT("Build target completed more than once.\nTarget: {}\nRound: {}",
                                 rb.getBTarget()->getPrintName(), static_cast<uint32_t>(rb.round)));
    }
    ++updatedCount;

    DEBUG_EXECUTE(FORMAT("{} Locking in try block {} {}\n", round, __LINE__, getThreadId()));
    if (rb.exitStatus != EXIT_SUCCESS)
    {
        errorHappenedInRoundMode = true;
    }

    const bool setToNeedsUpdate = rb.updateStatus == UpdateStatus::UPDATE_NEEDED;
    if (const BTarget *const target = rb.getBTarget(); rb.round == 0 && setToNeedsUpdate &&
                                                       rb.exitStatus == EXIT_SUCCESS && target->launchesProcess &&
                                                       target->buildFooterUpdated)
    {
        rb.completionTime =
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
    }

    rb.isCompleted = true;

    for (const RBTWithType rbt : rb.dependents)
    {
        if (isBlockingRelation(rbt.getRelationType()))
        {
            RealBTarget *dependent = rbt.getPointer();
            if (setToNeedsUpdate)
            {
                dependent->updateStatus = UpdateStatus::UPDATE_NEEDED;
            }
            if (rb.exitStatus != EXIT_SUCCESS)
            {
                dependent->exitStatus = EXIT_FAILURE;
            }
            if (!dependent->dependenciesSize)
            {
                printErrorMessage(FORMAT("Build dependency count underflow.\nCompleted dependency: {}\n"
                                         "Dependent target: {}\nRound: {}",
                                         rb.getBTarget()->getPrintName(), dependent->getBTarget()->getPrintName(),
                                         static_cast<uint32_t>(dependent->round)));
            }
            --dependent->dependenciesSize;
            if (!dependent->dependenciesSize)
            {
                uint32_t insertionIndex;
                readyBTargets.emplace(rbt.getPointer(), insertionIndex);
                dependent->insertionIndex = insertionIndex;
            }
        }
    }

    DEBUG_EXECUTE(FORMAT("{} {} Info: readyBTargets.size() {} readyBTargetsSizeGoal {} {}\n", round, __LINE__,
                         readyBTargets.size(), readyBTargetsSizeGoal, getThreadId()));
}
