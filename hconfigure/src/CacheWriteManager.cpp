
#include "CacheWriteManager.hpp"
#include "BTarget.hpp"
#include "CppTarget.hpp"
#include "Node.hpp"
#include "TargetCache.hpp"

/*
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
}*/