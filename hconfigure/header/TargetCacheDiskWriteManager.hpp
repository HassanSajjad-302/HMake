

#ifndef TARGETCACHEDISKWRITEMANAGER_HPP
#define TARGETCACHEDISKWRITEMANAGER_HPP

#ifdef USE_HEADER_UNITS
import <condition_variable>;
import "BTarget.hpp";
#else
#include "BTarget.hpp"
#include <condition_variable>
#endif

struct ColoredStringForPrint
{
    pstring msg;
    uint32_t color;
    bool isColored;
    ColoredStringForPrint(pstring _msg, uint32_t _color, bool _isColored);
};

struct PValueAndIndices
{
    PValue pValue;
    uint64_t index0;
    uint64_t index1;
    uint64_t index2;
    uint64_t index3;
    uint64_t index4;
    explicit PValueAndIndices(PValue _pValue, uint64_t _index0, uint64_t _index1, uint64_t _index2, uint64_t _index3,
                              uint64_t _index4);
    PValue &getTargetPValue() const;
    void copyToCentralTargetCache();
};

class TargetCacheDiskWriteManager : public BTarget
{
    mutex vecMutex;
    std::condition_variable vecCond{};
    std::unique_lock<std::mutex> vecLock{vecMutex, std::defer_lock_t{}};
    vector<ColoredStringForPrint> strCache;
    vector<ColoredStringForPrint> strCacheLocal;
    vector<PValueAndIndices> pValueCache;
    vector<PValueAndIndices> pValueCacheLocal;
    RAPIDJSON_DEFAULT_ALLOCATOR writeBuildCacheAllocator;
    std::thread diskWriteManagerThread;
    uint64_t nodesSizeBefore = 0;
    bool exitAfterThis = false;
    // TODO
    // Make this global
    vector<BTarget *> copyJsonBTargets;
    atomic<uint64_t> copyJsonBTargetsCount = 0;

  public:
    TargetCacheDiskWriteManager();
    void addNewBTargetInCopyJsonBTargetsCount(BTarget *bTarget);
    void writeNodesCacheIfNewNodesAdded();
    ~TargetCacheDiskWriteManager() override;
    void initialize();
    void start();
    void delayPrintAndAddPValue(pstring &str, PValue _pValue, uint64_t _index0 = UINT64_MAX,
                                uint64_t _index1 = UINT64_MAX, uint64_t _index2 = UINT64_MAX,
                                uint64_t _index3 = UINT64_MAX, uint64_t _index4 = UINT64_MAX);
    void delayPrintColorAndAddPValue(pstring &str, uint32_t color, PValue _pValue, uint64_t _index0 = UINT64_MAX,
                                     uint64_t _index1 = UINT64_MAX, uint64_t _index2 = UINT64_MAX,
                                     uint64_t _index3 = UINT64_MAX, uint64_t _index4 = UINT64_MAX);
    void startOperations();
    void endOperations();
    void delayPrint(pstring &str);
    void delayPrintColor(pstring &str, uint32_t color);
    void updateBTarget(Builder &builder, unsigned short round) override;
    void endOfRound(Builder &builder, unsigned short round) override;
};

inline TargetCacheDiskWriteManager *targetCacheDiskWriteManager;

#endif // TARGETCACHEDISKWRITEMANAGER_HPP
