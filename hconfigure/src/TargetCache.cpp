
#ifdef USE_HEADER_UNITS
import "TargetCache.hpp";
#else
#include "TargetCache.hpp"
#endif
#include <BuildSystemFunctions.hpp>

TargetCache::TargetCache(const pstring &name)
{
    const uint64_t index = pvalueIndexInSubArrayConsidered(configCache, PValue(ptoref(name)));

    if (bsMode == BSMode::CONFIGURE)
    {
        if (index == UINT64_MAX)
        {
            configCache.PushBack(PValue(kArrayType), ralloc);
            PValue *targetCacheLocal = &configCache[configCache.Size() - 1];
            targetCacheLocal->PushBack(PValue(kStringType).SetString(name.c_str(), name.size(), ralloc), ralloc);
            targetCacheLocal->PushBack(PValue(kArrayType), ralloc);
            buildCache.PushBack(PValue(kArrayType), ralloc);
            targetCacheIndex = configCache.Size() - 1;
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
    return configCache[targetCacheIndex][1];
}
PValue &TargetCache::getBuildCache() const
{
    return buildCache[targetCacheIndex];
}

void TargetCache::copyBackConfigCacheMutexLocked() const
{
    std::lock_guard _(configCacheMutex);
    getConfigCache().CopyFrom(buildOrConfigCacheCopy, ralloc);
}