
#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
import "TargetCacheDiskWriteManager.hpp";
import "Node.hpp";
#else
#include "TargetCacheDiskWriteManager.hpp"
#include "BTarget.hpp"
#include "Node.hpp"
#endif
#include "CppSourceTarget.hpp"
#include "TargetCache.hpp"

ColoredStringForPrint::ColoredStringForPrint(string _msg, const uint32_t _color, const bool _isColored)
    : msg(std::move(_msg)), color(_color), isColored(_isColored)
{
}

UpdatedCache::UpdatedCache(TargetCache *target_, void *cache_) : target(target_), cache(cache_)
{
}

TargetCacheDiskWriteManager::TargetCacheDiskWriteManager()
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        copyJsonBTargets.reserve(4096 * 4);
    }
}

void TargetCacheDiskWriteManager::writeNodesCacheIfNewNodesAdded()
{
    if (const uint64_t newNodesSize = atomic_ref(Node::idCountCompleted).load(); newNodesSize != nodesSizeBefore)
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

TargetCacheDiskWriteManager::~TargetCacheDiskWriteManager()
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        writeNodesCacheIfNewNodesAdded();
    }
    else
    {
        targetCacheDiskWriteManager.vecMutex.lock();
        targetCacheDiskWriteManager.exitAfterThis = true;
        targetCacheDiskWriteManager.vecMutex.unlock();
        targetCacheDiskWriteManager.vecCond.notify_one();
        diskWriteManagerThread.join();
    }
}

void TargetCacheDiskWriteManager::initialize()
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        // TODO
        // Allocate this and all the other globals in one function call.
        strCache.reserve(1000);
        strCacheLocal.reserve(1000);
        updatedCaches.reserve(1000);
        updatedCachesLocal.reserve(1000);
    }

    nodesSizeBefore = Node::idCountCompleted;
    nodesSizeStart = nodesSizeBefore;
}

void TargetCacheDiskWriteManager::performThreadOperations(const bool doUnlockAndRelock)
{
    if (!strCache.empty())
    {
        // Should be based on if a new node is entered.
        strCacheLocal.swap(strCache);
        updatedCachesLocal.swap(updatedCaches);
        strCache.clear();
        updatedCaches.clear();

        if (doUnlockAndRelock)
        {
            vecMutex.unlock();
        }

        writeNodesCacheIfNewNodesAdded();

        if (!updatedCachesLocal.empty())
        {
            for (const UpdatedCache &p : updatedCachesLocal)
            {
                p.target->updateBuildCache(p.cache);
            }

            writeBuildBuffer(buildBufferLocal);
            writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("build-cache"),
                                        buildBufferLocal);
        }
        // Copying value from array to central value

        for (ColoredStringForPrint &c : strCacheLocal)
        {
            if (c.isColored)
            {
                printMessageColor(c.msg, c.color);
            }
            else
            {
                printMessage(c.msg);
            }
        }

        if (doUnlockAndRelock)
        {
            vecMutex.lock();
        }
    }
}

void TargetCacheDiskWriteManager::start()
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

void TargetCacheDiskWriteManager::endOfRound()
{
    // This function is executed in the first thread. After that, the destructor of this manager is executed in this
    // thread which waits for the thread to finish.

    // This will still copy even if an error has happened. This will copy only in
    // round 1 hence only in BSMode::BUILD.
    writeNodesCacheIfNewNodesAdded();

    if (!copyJsonBTargets.empty())
    {
        for (CppSourceTarget *t : copyJsonBTargets)
        {
            t->checkAndCopyBuildCache();
        }
        writeBuildBuffer(buildBufferLocal);
        writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("build-cache"),
                                    buildBufferLocal);
    }

    diskWriteManagerThread = std::thread(&TargetCacheDiskWriteManager::start, &targetCacheDiskWriteManager);
}