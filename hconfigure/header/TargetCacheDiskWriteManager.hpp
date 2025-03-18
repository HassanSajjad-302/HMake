

#ifndef TARGETCACHEDISKWRITEMANAGER_HPP
#define TARGETCACHEDISKWRITEMANAGER_HPP

#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
import "BuildSystemFunctions.hpp";
import "rapidjson/document.h";
import <condition_variable>;
#else
#include "BTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "rapidjson/document.h"
#include <condition_variable>
#endif

using rapidjson::Value;
struct ColoredStringForPrint
{
    string msg;
    uint32_t color;
    bool isColored;
    ColoredStringForPrint(string _msg, uint32_t _color, bool _isColored);
};

struct ValueAndIndices
{
    Value value;
    uint64_t index0;
    uint64_t index1;
    uint64_t index2;
    uint64_t index3;
    uint64_t index4;
    explicit ValueAndIndices(Value _value, uint64_t _index0, uint64_t _index1, uint64_t _index2, uint64_t _index3,
                              uint64_t _index4);
    Value &getTargetValue() const;
    void copyToCentralTargetCache();
};

class TargetCacheDiskWriteManager : public BTarget
{
  public:
    mutex vecMutex;
    std::condition_variable vecCond{};
    std::unique_lock<std::mutex> vecLock{vecMutex, std::defer_lock_t{}};
    vector<ColoredStringForPrint> strCache;
    vector<ValueAndIndices> valueCache;

  private:
    vector<ColoredStringForPrint> strCacheLocal;
    vector<ValueAndIndices> valueCacheLocal;

  public:
    vector<BTarget *> copyJsonBTargets;
    RAPIDJSON_DEFAULT_ALLOCATOR writeBuildCacheAllocator;
    std::thread diskWriteManagerThread;
    uint64_t nodesSizeBefore = 0;
    uint64_t nodesSizeStart = 0;
    bool exitAfterThis = false;
    // TODO
    // Make this global
    atomic<uint64_t> copyJsonBTargetsCount = 0;

    TargetCacheDiskWriteManager();
    void addNewBTargetInCopyJsonBTargetsCount(BTarget *bTarget);
    void writeNodesCacheIfNewNodesAdded();
    ~TargetCacheDiskWriteManager() override;
    void initialize();
    void performThreadOperations(bool doUnlockAndRelock);
    void start();

    void updateBTarget(Builder &builder, unsigned short round) override;
    void endOfRound(Builder &builder, unsigned short round) override;
};

GLOBAL_VARIABLE(TargetCacheDiskWriteManager, targetCacheDiskWriteManager)

#endif // TARGETCACHEDISKWRITEMANAGER_HPP
