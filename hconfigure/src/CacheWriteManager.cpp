
#include "CacheWriteManager.hpp"
#include "BTarget.hpp"
#include "CppSourceTarget.hpp"
#include "Node.hpp"
#include "TargetCache.hpp"

ColoredStringForPrint::ColoredStringForPrint(string _msg, const uint32_t _color, const bool _isColored)
    : msg(std::move(_msg)), color(_color), isColored(_isColored)
{
}

UpdatedCache::UpdatedCache(TargetCache *target_, void *cache_) : target(target_), cache(cache_)
{
}

void CacheWriteManager::addNewEntry(TargetCache *target, void *cache)
{
    {
        std::lock_guard _(cacheWriteManager.vecMutex);
        cacheWriteManager.updatedCaches.emplace_back(target, cache);
    }
    cacheWriteManager.vecCond.notify_one();
}

void CacheWriteManager::writeNodesCacheIfNewNodesAdded()
{
    if (const uint64_t newNodesSize = atomic_ref(Node::idCountCompleted).load(std::memory_order_acquire);
        newNodesSize != nodesSizeBefore)
    {
        // printMessage(FORMAT("nodesSizeStart {} nodesSizeBefore {} nodesSizeAfter {}\n", nodesSizeStart,
        //                          nodesSizeBefore, newNodesSize));
        for (uint64_t i = nodesSizeBefore; i < newNodesSize; ++i)
        {
            const string &str = Node::nodeIndices[i]->filePath;
            uint16_t strSize = str.size();
            const auto ptr = reinterpret_cast<const char *>(&strSize);
            nodesCacheGlobal.insert(nodesCacheGlobal.end(), ptr, ptr + 2);
            nodesCacheGlobal.insert(nodesCacheGlobal.end(), str.begin(), str.end());
        }
        nodesSizeBefore = newNodesSize;
        writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("nodes"), nodesCacheGlobal);
    }
}

CacheWriteManager::~CacheWriteManager()
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        writeNodesCacheIfNewNodesAdded();
    }
    else
    {
        cacheWriteManager.vecMutex.lock();
        cacheWriteManager.exitAfterThis = true;
        cacheWriteManager.vecMutex.unlock();
        cacheWriteManager.vecCond.notify_one();
        diskWriteManagerThread.join();
    }
}

void CacheWriteManager::initialize()
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        // TODO
        // Allocate this and all the other globals in one function call.
        updatedCaches.reserve(1000);
        updatedCachesLocal.reserve(1000);
    }

    nodesSizeBefore = Node::idCountCompleted;
    nodesSizeStart = nodesSizeBefore;
}

void CacheWriteManager::performThreadOperations(const bool doUnlockAndRelock)
{
    if (!updatedCaches.empty())
    {
        // Should be based on if a new node is entered.
        updatedCachesLocal.swap(updatedCaches);
        updatedCaches.clear();

        if (doUnlockAndRelock)
        {
            vecMutex.unlock();
        }

        bool buildCacheModified = false;
        for (const UpdatedCache &p : updatedCachesLocal)
        {
            p.target->updateBuildCache(p.cache, outputStr, errorStr, buildCacheModified);
        }

        if (buildCacheModified)
        {
            writeNodesCacheIfNewNodesAdded();
            writeBuildBuffer(buildBufferLocal);
            writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("build-cache"),
                                        buildBufferLocal);
        }

        if (!updatedCachesLocal.empty())
        {
            fmt::print("{}", outputStr);
            fmt::print(stderr, "{}", errorStr);
            outputStr.clear();
            errorStr.clear();
        }

        if (doUnlockAndRelock)
        {
            vecMutex.lock();
        }
    }
}

void CacheWriteManager::start()
{
    vecMutex.lock();

    while (true)
    {
        performThreadOperations(true);
        if (exitAfterThis)
        {
            // Need to do this because other threads might have appended new data while this thread was performing
            // operations in mutex unlocked state.
            // But do not need to do unlocking and re-locking since we are exiting after this.
            performThreadOperations(false);
            break;
        }
        vecCond.wait(vecLock);
    }
}

void CacheWriteManager::endOfRound()
{
    // This function is executed in the first thread. After that, the destructor of this manager is executed in this
    // thread which waits for the thread to finish.

    // This will still copy even if an error has happened. This will copy only in
    // round 1 hence only in BSMode::BUILD.
    diskWriteManagerThread = std::thread(&CacheWriteManager::start, &cacheWriteManager);
}