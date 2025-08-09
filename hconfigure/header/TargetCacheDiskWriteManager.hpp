

#ifndef TARGETCACHEDISKWRITEMANAGER_HPP
#define TARGETCACHEDISKWRITEMANAGER_HPP

#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp";
import "rapidjson/document.h";
import <atomic>;
import <condition_variable>;
#else
#include "BuildSystemFunctions.hpp"
#include "rapidjson/document.h"
#include <atomic>
#include <condition_variable>
#endif
#include "TargetCache.hpp"

using std::atomic;

using rapidjson::Value;
struct ColoredStringForPrint
{
    string msg;
    uint32_t color;
    bool isColored;
    ColoredStringForPrint(string _msg, uint32_t _color, bool _isColored);
};

struct UpdatedCache
{
    BuildCache::Cpp::ModuleFile cache;
    CppSourceTarget *target;
    bool isSource;
    UpdatedCache(CppSourceTarget *target_, BuildCache::Cpp::ModuleFile cache_, bool isSource_);
};

class TargetCacheDiskWriteManager
{
  public:
    mutex vecMutex;
    std::condition_variable vecCond{};
    std::unique_lock<std::mutex> vecLock{vecMutex, std::defer_lock_t{}};
    vector<ColoredStringForPrint> strCache;
    vector<UpdatedCache> updatedCaches;

  private:
    vector<ColoredStringForPrint> strCacheLocal;
    vector<UpdatedCache> updatedCachesLocal;

  public:
    vector<CppSourceTarget *> copyJsonBTargets;
    vector<char> buildBuffer;
    RAPIDJSON_DEFAULT_ALLOCATOR writeBuildCacheAllocator;
    std::thread diskWriteManagerThread;
    uint64_t nodesSizeBefore = 0;
    uint64_t nodesSizeStart = 0;
    bool exitAfterThis = false;
    // TODO
    // Make this global
    atomic<uint64_t> cppSourceTargets = 0;

    TargetCacheDiskWriteManager();
    void updateCacheOnRoundEndCppSourceTarget(CppSourceTarget *target);
    void writeNodesCacheIfNewNodesAdded();
    ~TargetCacheDiskWriteManager();
    void initialize();
    void performThreadOperations(bool doUnlockAndRelock);
    void start();
    void endOfRound();
};

GLOBAL_VARIABLE(TargetCacheDiskWriteManager, targetCacheDiskWriteManager)

#endif // TARGETCACHEDISKWRITEMANAGER_HPP
