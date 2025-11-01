

#ifndef CACHEWRITEMANAGER_HPP
#define CACHEWRITEMANAGER_HPP

#include "BuildSystemFunctions.hpp"
#include "TargetCache.hpp"
#include <atomic>
#include <condition_variable>

using std::atomic;

struct ColoredStringForPrint
{
    string msg;
    uint32_t color;
    bool isColored;
    ColoredStringForPrint(string _msg, uint32_t _color, bool _isColored);
};

struct UpdatedCache
{
    TargetCache *target;
    void *cache;
    UpdatedCache(TargetCache *target_, void *cache_);
};

class CacheWriteManager
{
  public:
    mutex vecMutex;
    std::condition_variable vecCond{};
    std::unique_lock<std::mutex> vecLock{vecMutex, std::defer_lock_t{}};
    vector<UpdatedCache> updatedCaches;

  private:
    string outputStr;
    string errorStr;
    vector<UpdatedCache> updatedCachesLocal;
    vector<char> buildBufferLocal;

  public:
    vector<char> buildBuffer;
    std::thread diskWriteManagerThread;
    uint64_t nodesSizeBefore = 0;
    uint64_t nodesSizeStart = 0;
    bool exitAfterThis = false;

    static void addNewEntry(TargetCache *target, void *cache);
    void writeNodesCacheIfNewNodesAdded();
    ~CacheWriteManager();
    void initialize();
    void performThreadOperations(bool doUnlockAndRelock);
    void start();
    void endOfRound();
};

GLOBAL_VARIABLE(CacheWriteManager, cacheWriteManager)

#endif // CACHEWRITEMANAGER_HPP
