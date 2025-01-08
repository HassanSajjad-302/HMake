
#ifdef USE_HEADER_UNITS
import "TargetCache.hpp";
#else
#include "TargetCache.hpp"
#endif
#include <BuildSystemFunctions.hpp>

TargetCache::TargetCache(const pstring &name)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const uint64_t index = pvalueIndexInSubArray(configCache, PValue(ptoref(name)));
        if (index == UINT64_MAX)
        {
            configCache.PushBack(PValue(kArrayType), ralloc);
            buildCache.PushBack(PValue(kArrayType), ralloc);
            targetCacheIndex = configCache.Size() - 1;
            configCache[targetCacheIndex].PushBack(PValue(kStringType).SetString(name.c_str(), name.size(), ralloc),
                                                   ralloc);
        }
        else
        {
            targetCacheIndex = index;
            getConfigCache().Clear();
            configCache[targetCacheIndex].PushBack(PValue(kStringType).SetString(name.c_str(), name.size(), ralloc),
                                                   ralloc);
        }

#ifndef BUILD_MODE
        myId = idCount;
        ++idCount;
        if (auto [pos, ok] = targetCacheIndexAndMyIdHashMap.emplace(targetCacheIndex, myId); !ok)
        {
            printErrorMessage(
                fmt::format("Attempting to add 2 targets with same name {} in config-cache.json\n", name));
            exit(EXIT_FAILURE);
        }

#endif
        buildOrConfigCacheCopy.PushBack(PValue(kStringType).SetString(name.c_str(), name.size(), ralloc), ralloc);
    }
    else
    {
        const uint64_t index = pvalueIndexInSubArrayConsidered(configCache, PValue(ptoref(name)));
        if (index != UINT64_MAX)
        {
            targetCacheIndex = index;
            buildOrConfigCacheCopy = PValue().CopyFrom(getBuildCache(), cacheAlloc);
        }
        else
        {
            printErrorMessage(fmt::format(
                "Target {} not found in config-cache.\nMaybe you need to run hhelper first to update the target-cache.",
                name));
            exit(EXIT_FAILURE);
        }
    }
    flat_hash_map<int, int> a;
    a.emplace(2, 3);
}

PValue &TargetCache::getConfigCache() const
{
    return configCache[targetCacheIndex];
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