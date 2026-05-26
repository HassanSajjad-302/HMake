
#include "Builder.hpp"
#include "Cache.hpp"
#include "CppMod.hpp"
#include "JConsts.hpp"
#include "Manager.hpp"
#include "Node.hpp"
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
    checkNodes(true);
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
            rb->getBTarget()->generateStandAloneCommand();
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

static void checkDepsChanged(RealBTarget &rb)
{
    const uint32_t cacheIdx = rb.getBTarget()->cacheIndex;
    BTargetCache &fc = bTargetCaches[cacheIdx];

    string *const newDeps = new string();
    newDeps->reserve(4 + 4 * rb.dependenciesSize);
    writeUint32(*newDeps, rb.dependenciesSize);
    for (const RBTWithType &rbt : rb.dependencies)
    {
        if (const RelationType kind = rbt.getRelationType(); kind == RelationType::FULL || kind == RelationType::WAIT)
        {
            writeUint32(*newDeps, rbt.getPointer()->getBTarget()->cacheIndex);
        }
    }
    fc.depsCache = *newDeps;
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
            RealBTarget &rb = *RealBTarget::sorted[i];

            rb.indexInTopologicalSort = topSize - (i + 1);

            if (rb.getBTarget()->selectiveBuild)
            {
                // We need to check if our rb.dependenciesSize == 0, because it may have one in the deps-cache
                if (rb.checkDepsChanged())
                {
                    const uint32_t cacheIdx = rb.getBTarget()->cacheIndex;
                    BTargetCache &fc = bTargetCaches[cacheIdx];

                    string *const newDeps = new string();
                    newDeps->reserve(4 + 4 * rb.dependenciesSize);
                    writeUint32(*newDeps, rb.dependenciesSize);
                    rb.updateStatus = UpdateStatus::UPDATE_NEEDED;

                    for (const auto &rbt : rb.dependencies)
                    {
                        const RelationType relationType = rbt.getRelationType();
                        BTarget *dep = rbt.getPointer()->getBTarget();

                        if (relationType == RelationType::FULL || relationType == RelationType::SELECTIVE)
                        {
                            dep->selectiveBuild = true;
                        }

                        if (relationType == RelationType::FULL || relationType == RelationType::WAIT)
                        {
                            writeUint32(*newDeps, dep->cacheIndex);
                        }
                    }
                    fc.depsCache = *newDeps;
                }
                else
                {
                    for (const RBTWithType &rbt : rb.dependencies)
                    {
                        if (rbt.getRelationType() == RelationType::FULL ||
                            rbt.getRelationType() == RelationType::SELECTIVE)
                        {
                            rbt.getPointer()->getBTarget()->selectiveBuild = true;
                        }
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
    uint16_t numberOfLaunchedThreads = cache.numberOfBuildProcesses;
    // numberOfLaunchedThreads = 1;
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
            const uint64_t pid = b->getBTarget()->run.pid;

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

            if (b->getBTarget()->isEventRegistered(*this))
            {
                --idleCount;
            }
            else
            {
                decrementFromDependents(const_cast<RealBTarget &>(*b));
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

                    const string buildCache = getBuildCache();
                    writeNodesCacheIfNewNodesAdded();
                    writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("build-cache"),
                                                buildCache);

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
            // At this point the list must be empty
            if (updateBTargets.hasElement())
            {
                HMAKE_HMAKE_INTERNAL_ERROR
            }
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

template <typename T> void divideInChunk(std::pmr::vector<std::span<T>> &result, std::pmr::vector<T> &v, uint16_t n)
{

    // If n is 1, return vector containing one span of the entire vector
    if (n == 1)
    {
        result.emplace_back(std::span<T>(v.data(), v.size()));
        return;
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

        return;
    }

    // Normal case: divide vector into n chunks
    const size_t chunk_size = v.size() / n;
    const size_t remainder = v.size() % n;
    size_t start_pos = 0;

    for (uint16_t i = 0; i < n; ++i)
    {
        // First 'remainder' chunks get an extra element
        size_t current_chunk_size = chunk_size + (i < remainder ? 1 : 0);

        result.emplace_back(v.data() + start_pos, current_chunk_size);
        start_pos += current_chunk_size;
    }
}

void Builder::checkNodes(const bool isFirstTime)
{
    STACK_PMR_VECTOR(Node *, statNodes, 256 * 1024)
    STACK_PMR_VECTOR(Node *, hashNodes, 256 * 1024)

    if (isFirstTime)
    {
        for (uint32_t i = 0; i < Node::idCount; ++i)
        {
            Node *node = nodeIndices[i];
            if (node->doStatFile || node->doHashFile)
            {
                statNodes.emplace_back(node);
            }
            if (node->doHashFile)
            {
                hashNodes.emplace_back(node);
            }
        }
    }
    else
    {
        // This is called at the end before saving the cache. Here we cached those that were skipped earlier ( were
        // generated during the build or initially there toBeChecked was false ( like dynamically discovered
        // header-files)).

        for (uint32_t i = 0; i < Node::idCount; ++i)
        {
            if (Node *node = nodeIndices[i]; node->doHashFile && !node->hashCompleted)
            {
                statNodes.emplace_back(node);
                hashNodes.emplace_back(node);
            }
        }
    }

    const uint32_t hwc = [] {
        const uint32_t n = std::thread::hardware_concurrency();
        return n ? n : 1;
    }();

    // Phase 1: stat — uniform cost, static chunks are fine.
    if (statNodes.empty())
    {
        return;
    }
    {
        STACK_PMR_VECTOR(std::span<Node *>, chunks, 4 * 1024)
        const uint32_t workerCount = std::min<uint32_t>(hwc, statNodes.size());
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

    // Phase 2: hash — heterogeneous cost, LPT static assignment.
    if (hashNodes.empty())
    {
        return;
    }

    // Partition not_found nodes to the end without shifting.
    // Swap-with-last is O(n) and allocation-free.
    uint32_t validCount = static_cast<uint32_t>(hashNodes.size());
    for (uint32_t i = 0; i < validCount;)
    {
        if (hashNodes[i]->fileType == file_type::not_found)
        {
            hashNodes[i]->contentHash = 0;
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
    RealBTarget *rb = updateBTargets.getItem();

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
        rb = updateBTargets.getItem();
    }
}

void Builder::decrementFromDependents(RealBTarget &rb)
{
    DEBUG_EXECUTE(FORMAT("{} Locking in try block {} {}\n", round, __LINE__, getThreadId()));
    if (rb.exitStatus != EXIT_SUCCESS)
    {
        errorHappenedInRoundMode = true;
    }

    const bool setToNeedsUpdate = rb.updateStatus == UpdateStatus::UPDATE_NEEDED;
    rb.isCompleted = true;

    for (const RBTWithType rbt : rb.dependents)
    {
        if (rbt.getRelationType() == RelationType::FULL)
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
            dependent->dependenciesSize -= 1;
            if (!dependent->dependenciesSize)
            {
                uint32_t insertionIndex;
                updateBTargets.emplace(rbt.getPointer(), insertionIndex);
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