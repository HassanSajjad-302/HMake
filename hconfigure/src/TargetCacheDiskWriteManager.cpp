
#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
import "TargetCacheDiskWriteManager.hpp";
import "Node.hpp";
#else
#include "TargetCacheDiskWriteManager.hpp"
#include "BTarget.hpp"
#include "Node.hpp"
#endif

ColoredStringForPrint::ColoredStringForPrint(pstring _msg, uint32_t _color, bool _isColored)
    : msg(std::move(_msg)), color(_color), isColored(_isColored)
{
}
PValueAndIndices::PValueAndIndices(PValue _pValue, const uint64_t _index0, const uint64_t _index1,
                                   const uint64_t _index2, const uint64_t _index3, const uint64_t _index4)

    : pValue{std::move(_pValue)}, index0{_index0}, index1{_index1}, index2{_index2}, index3{_index3}, index4{_index4}
{
}

PValue &PValueAndIndices::getTargetPValue() const
{
    PValue &target0 = targetCache;
    if (index0 == UINT64_MAX)
    {
        return target0;
    }
    PValue &target1 = target0[index0];
    if (index1 == UINT64_MAX)
    {
        return target1;
    }
    PValue &target2 = target1[index1];
    if (index2 == UINT64_MAX)
    {
        return target2;
    }
    PValue &target3 = target2[index2];
    if (index3 == UINT64_MAX)
    {
        return target3;
    }
    PValue &target4 = target3[index3];
    if (index4 == UINT64_MAX)
    {
        return target4;
    }
    return target4[index4];
}

TargetCacheDiskWriteManager::TargetCacheDiskWriteManager()
{
    copyJsonBTargets.reserve(10000);
}

void TargetCacheDiskWriteManager::addNewBTargetInCopyJsonBTargetsCount(BTarget *bTarget)
{
    const uint64_t i = copyJsonBTargetsCount.fetch_add(1);
    copyJsonBTargets[i] = bTarget;
}

void TargetCacheDiskWriteManager::writeNodesCacheIfNewNodesAdded()
{
    if (const uint64_t newNodesSize = Node::idCount.load(); newNodesSize != nodesSizeBefore)
    {
        for (uint64_t i = nodesSizeBefore; i < newNodesSize; ++i)
        {
            nodesCacheJson.PushBack(PValue(nodesCacheVector[i].data(), nodesCacheVector[i].size()), ralloc);
        }
        nodesSizeBefore = newNodesSize;
        writePValueToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("nodes"), nodesCacheJson);
    }
}

TargetCacheDiskWriteManager::~TargetCacheDiskWriteManager()
{
    if (bsMode == BSMode::CONFIGURE)
    {
        writeNodesCacheIfNewNodesAdded();
    }
}

void TargetCacheDiskWriteManager::initialize()
{
    if (bsMode == BSMode::BUILD)
    {
        // TODO
        // Allocate this and all the other globals in one function call.
        strCache.reserve(1000);
        strCacheLocal.reserve(1000);
        pValueCache.reserve(1000);
        pValueCacheLocal.reserve(1000);
    }

    nodesSizeBefore = nodesCacheJson.Size();
}

void TargetCacheDiskWriteManager::start()
{
    while (true)
    {
        vecLock.lock();
        vecCond.wait(vecLock);
        if (!strCache.empty())
        {
            // Should be based on if a new node is entered.
            strCacheLocal = std::move(strCache);
            pValueCacheLocal = std::move(pValueCache);
            strCache.clear();
            pValueCache.clear();
            vecLock.unlock();

            writeNodesCacheIfNewNodesAdded();

            // Copying pvalue from array to central pvalue
            for (PValueAndIndices &p : pValueCacheLocal)
            {
                p.getTargetPValue() = std::move(p.pValue);
            }

            writePValueToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("target-cache"),
                                        targetCache);

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

            if (exitAfterThis)
            {
                break;
            }
        }
        else
        {
            vecLock.unlock();
        }
    }
}

void TargetCacheDiskWriteManager::delayPrintAndAddPValue(pstring &str, PValue _pValue, uint64_t _index0,
                                                         uint64_t _index1, uint64_t _index2, uint64_t _index3,
                                                         uint64_t _index4)
{
    lock_guard _{vecMutex};
    pValueCache.emplace_back(std::move(_pValue), _index0, _index1, _index2, _index3, _index4);
    strCache.emplace_back(str, 0, false);
}

void TargetCacheDiskWriteManager::delayPrintColorAndAddPValue(pstring &str, uint32_t color, PValue _pValue,
                                                              uint64_t _index0, uint64_t _index1, uint64_t _index2,
                                                              uint64_t _index3, uint64_t _index4)
{
    lock_guard _{vecMutex};
    pValueCache.emplace_back(std::move(_pValue), _index0, _index1, _index2, _index3, _index4);
    strCache.emplace_back(str, color, true);
}

void TargetCacheDiskWriteManager::startOperations()
{
    diskWriteManagerThread = std::thread(&TargetCacheDiskWriteManager::start, targetCacheDiskWriteManager);
}

void TargetCacheDiskWriteManager::endOperations()
{
    targetCacheDiskWriteManager->exitAfterThis = true;
    targetCacheDiskWriteManager->vecCond.notify_one();
    diskWriteManagerThread.join();
}

void TargetCacheDiskWriteManager::delayPrint(pstring &str)
{
    lock_guard _{vecMutex};
    strCache.emplace_back(str, 0, false);
}

void TargetCacheDiskWriteManager::delayPrintColor(pstring &str, uint32_t color)
{
    lock_guard _{vecMutex};
    strCache.emplace_back(str, color, true);
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
    // This will still copy even if an error has happened.
    // This will only copy only in round 1 hence only in BSMode::BUILD.
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
            writePValueToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("target-cache"),
                                        targetCache);
        }

        targetCacheDiskWriteManager->startOperations();
    }
}
