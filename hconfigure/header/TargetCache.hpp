#ifndef TARGETCACHE_HPP
#define TARGETCACHE_HPP

#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp";
#else
#include "BuildSystemFunctions.hpp"
#endif

struct TargetCache
{
    PValue buildOrConfigCacheCopy{kArrayType};
    uint64_t targetCacheIndex = UINT64_MAX;
    RAPIDJSON_DEFAULT_ALLOCATOR cacheAlloc;
    explicit TargetCache(const pstring &name);
    PValue &getConfigCache() const;
    PValue &getBuildCache() const;
    void copyBackConfigCacheMutexLocked() const;
};
#endif // TARGETCACHE_HPP
