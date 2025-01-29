#ifndef TARGETCACHE_HPP
#define TARGETCACHE_HPP

#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp";
#else
#include "BuildSystemFunctions.hpp"
#include "parallel-hashmap/parallel_hashmap/phmap.h"
#endif

using phmap::flat_hash_map;
struct TargetCache
{
#ifndef BUILD_MODE
    inline static uint64_t idCount = 0;
    inline static flat_hash_map<uint64_t, uint64_t> targetCacheIndexAndMyIdHashMap;
    uint64_t myId = 0;
#endif
    Value buildOrConfigCacheCopy{kArrayType};
    uint64_t targetCacheIndex = UINT64_MAX;
    RAPIDJSON_DEFAULT_ALLOCATOR cacheAlloc;
    explicit TargetCache(const string &name);
    Value &getConfigCache() const;
    Value &getBuildCache() const;
    void copyBackConfigCacheMutexLocked() const;
};
#endif // TARGETCACHE_HPP
