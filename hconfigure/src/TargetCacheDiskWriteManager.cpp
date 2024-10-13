
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
    strCache.reserve(1000);
    strCacheLocal.reserve(1000);
}

void TargetCacheDiskWriteManager::start()
{
    while (true)
    {
        vecLock.lock();
        vecCond.wait(vecLock);
        if (!strCache.empty())
        {
            PDocument nodesCacheLocal, targetCacheLocal;
            // Should be based on if a new node is entered.
            nodesCacheLocal.CopyFrom(nodesCacheJson, writeBuildCacheAllocator);
            targetCacheLocal.CopyFrom(targetCache, writeBuildCacheAllocator);
            strCacheLocal = std::move(strCache);
            pValueCacheLocal = std::move(pValueCache);
            strCache.clear();
            pValueCache.clear();
            vecLock.unlock();

            // Copying pvalue from array to central pvalue
            for (PValueAndIndices &p : pValueCacheLocal)
            {
                p.getTargetPValue() = std::move(p.pValue);
            }

            writePValueToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("nodes"),
                                        nodesCacheLocal);
            writePValueToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("target-cache"),
                                        targetCacheLocal);
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
    diskWriteManagerThread = std::thread(&TargetCacheDiskWriteManager::start, &targetCacheDiskWriteManager);
}

void TargetCacheDiskWriteManager::endOperations()
{
    targetCacheDiskWriteManager.exitAfterThis = true;
    targetCacheDiskWriteManager.vecCond.notify_one();
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

void TargetCacheDiskWriteManager::updateBTarget(Builder &builder, unsigned short round)
{
    if (round == 2)
    {
        uint64_t i = endStageTargetsCount.fetch_add(1);
        endStageTargets[i] = this;
    }
}

void TargetCacheDiskWriteManager::endOfRound(Builder &builder, unsigned short round)
{
    // TODO
    // If targetCache is modified, write the nodes.cache and then target-cache.json.
}
