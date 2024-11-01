
#ifdef USE_HEADER_UNITS
import "TargetCache.hpp";
#else
#include "TargetCache.hpp"
#endif
#include <BuildSystemFunctions.hpp>

TargetCache::TargetCache(const pstring &name)
{
    const uint64_t index = pvalueIndexInSubArrayConsidered(targetCache, PValue(ptoref(name)));

    if (bsMode == BSMode::CONFIGURE)
    {
        if (index == UINT64_MAX)
        {
            targetCache.PushBack(PValue(kArrayType), ralloc);
            PValue *targetCacheLocal = &targetCache[targetCache.Size() - 1];
            targetCacheLocal->PushBack(PValue(kStringType).SetString(name.c_str(), name.size(), ralloc), ralloc);
            targetCacheLocal->PushBack(PValue(kArrayType), ralloc);
            targetCacheLocal->PushBack(PValue(kArrayType), ralloc);
            targetCacheIndex = targetCache.Size() - 1;
        }
        else
        {
            targetCacheIndex = index;
            getConfigCache().Clear();
        }
    }
    else
    {
        if (index != UINT64_MAX)
        {
            targetCacheIndex = index;
            buildOrConfigCacheCopy = PValue().CopyFrom(getBuildCache(), cacheAlloc);
        }
        else
        {
            printErrorMessage(fmt::format(
                "Target {} not found in target-cache.\nMaybe you need to run hhelper first to update the target-cache.",
                name));
            exit(EXIT_FAILURE);
        }
    }
}

PValue &TargetCache::getConfigCache() const
{
    return targetCache[targetCacheIndex][1];
}
PValue &TargetCache::getBuildCache() const
{
    return targetCache[targetCacheIndex][2];
}

void TargetCache::copyBackConfigCacheMutexLocked() const
{
    std::lock_guard _(buildOrConfigCacheMutex);
    getConfigCache().CopyFrom(buildOrConfigCacheCopy, ralloc);
}