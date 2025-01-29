
#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
import "TargetCacheDiskWriteManager.hpp";
import "Node.hpp";
#else
#include "TargetCacheDiskWriteManager.hpp"
#include "BTarget.hpp"
#include "Node.hpp"
#endif

ColoredStringForPrint::ColoredStringForPrint(string _msg, uint32_t _color, bool _isColored)
    : msg(std::move(_msg)), color(_color), isColored(_isColored)
{
}
ValueAndIndices::ValueAndIndices(Value _pValue, const uint64_t _index0, const uint64_t _index1,
                                   const uint64_t _index2, const uint64_t _index3, const uint64_t _index4)

    : pValue{std::move(_pValue)}, index0{_index0}, index1{_index1}, index2{_index2}, index3{_index3}, index4{_index4}
{
}

Value &ValueAndIndices::getTargetValue() const
{
    Value &target0 = buildCache;
    if (index0 == UINT64_MAX)
    {
        return target0;
    }
    Value &target1 = target0[index0];
    if (index1 == UINT64_MAX)
    {
        return target1;
    }
    Value &target2 = target1[index1];
    if (index2 == UINT64_MAX)
    {
        return target2;
    }
    Value &target3 = target2[index2];
    if (index3 == UINT64_MAX)
    {
        return target3;
    }
    Value &target4 = target3[index3];
    if (index4 == UINT64_MAX)
    {
        return target4;
    }
    return target4[index4];
}

TargetCacheDiskWriteManager::TargetCacheDiskWriteManager()
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        copyJsonBTargets.reserve(10000);
#ifdef NDEBUG
        std::memset(copyJsonBTargets.data(), 0, 10000 * sizeof(void *));
#else
        // satisify the sanitizer and iterator based debuggerr
        for (int i = 0; i < 10000; ++i)
        {
            copyJsonBTargets.emplace_back(nullptr);
        }
#endif
    }
}

void TargetCacheDiskWriteManager::addNewBTargetInCopyJsonBTargetsCount(BTarget *bTarget)
{
    const uint64_t i = copyJsonBTargetsCount.fetch_add(1);
    copyJsonBTargets[i] = bTarget;
}

void TargetCacheDiskWriteManager::writeNodesCacheIfNewNodesAdded()
{
    if (const uint64_t newNodesSize = Node::idCountCompleted.load(); newNodesSize != nodesSizeBefore)
    {
        // printMessage(FORMAT("nodesSizeStart {} nodesSizeBefore {} nodesSizeAfter {}\n", nodesSizeStart,
        //                          nodesSizeBefore, newNodesSize));
        for (uint64_t i = nodesSizeBefore; i < newNodesSize; ++i)
        {
            nodesCacheJson.PushBack(
                Value(Node::nodeIndices[i]->filePath.c_str(), Node::nodeIndices[i]->filePath.size()), ralloc);
        }
        nodesSizeBefore = newNodesSize;
        writeValueToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("nodes"), nodesCacheJson);
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
        pValueCache.reserve(1000);
        pValueCacheLocal.reserve(1000);
    }

    nodesSizeBefore = nodesCacheJson.Size();
    nodesSizeStart = nodesSizeBefore;
}

void TargetCacheDiskWriteManager::performThreadOperations(bool doUnlockAndRelock)
{
    if (!strCache.empty())
    {
        // Should be based on if a new node is entered.
        strCacheLocal.swap(strCache);
        pValueCacheLocal.swap(pValueCache);
        strCache.clear();
        pValueCache.clear();

        if (doUnlockAndRelock)
        {

            vecMutex.unlock();
        }

        writeNodesCacheIfNewNodesAdded();

        if (!pValueCacheLocal.empty())
        {
            for (ValueAndIndices &p : pValueCacheLocal)
            {
                p.getTargetValue() = std::move(p.pValue);
            }
            writeValueToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("build-cache"),
                                        buildCache);
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

void TargetCacheDiskWriteManager::updateBTarget(Builder &builder, const unsigned short round)
{
    if (round == 1)
    {
        const uint64_t i = roundEndTargetsCount.fetch_add(1);
        roundEndTargets[i] = this;
    }
}

void TargetCacheDiskWriteManager::endOfRound(Builder &builder, unsigned short round)
{
    // This function is executed in first thread. After that the destructor of this manager is excuted in this thread
    // which waits for the thread to finish.

    // This will still copy even if an error has happened. This will copy only in
    // round 1 hence only in BSMode::BUILD.
    if (round == 1)
    {
        writeNodesCacheIfNewNodesAdded();

        if (const uint64_t s = copyJsonBTargetsCount.load())
        {
            for (uint64_t i = 0; i < s; ++i)
            {
                copyJsonBTargets[i]->copyJson();
                copyJsonBTargets[i] = nullptr;
            }
            writeValueToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("build-cache"),
                                        buildCache);
        }

        diskWriteManagerThread = std::thread(&TargetCacheDiskWriteManager::start, &targetCacheDiskWriteManager);
    }
}