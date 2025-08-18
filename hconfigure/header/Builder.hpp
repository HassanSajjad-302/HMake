#ifndef HMAKE_BUILDER_HPP
#define HMAKE_BUILDER_HPP
#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
import "BuildSystemFunctions.hpp";
import <condition_variable>;
import <vector>;
import <list>;
#else
#include "BTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include <condition_variable>
#include <list>
#include <vector>
#endif

using std::vector, std::list;

class BTargetsArrayList
{
    struct ArrayListItem
    {
        BTarget *value;
        uint32_t next;
    };

    ArrayListItem *array = nullptr;
    uint32_t currentIndex = 0;
    // uint32_t first = UINT32_MAX;
    uint32_t last = UINT32_MAX;
    uint32_t arraySize = 0;

  public:
    void clear()
    {
        last = UINT32_MAX;
        arraySize = 0;
        // TODO
        // arraySize should be recorded to report max array size to not get into memory issues.
    }

    uint32_t size() const
    {
        return arraySize;
    }

    BTargetsArrayList()
    {
        array = new ArrayListItem[1024 * 1024];
    }

    void emplaceBackBeforeRound(BTarget *bTarget)
    {
        array[arraySize].value = bTarget;
        array[arraySize].next = UINT32_MAX;
        last = arraySize;
        if (arraySize)
        {
            // array is not empty, so making the last array element point to the newly inserted one.
            array[arraySize - 1].next = arraySize;
        }
        ++arraySize;
    }

    void emplaceFrontBeforeLastRound(BTarget *bTarget)
    {
        array[arraySize].value = bTarget;
        if (last == UINT32_MAX)
        {
            last = arraySize;
            array[arraySize].next = UINT32_MAX;
        }
        else
        {
            // array is not empty, so making the newly inserted element point to the last one.
            array[arraySize].next = arraySize - 1;
        }
        ++arraySize;
    }

    void initializeForRound(const uint8_t round)
    {
        if (round)
        {
            currentIndex = 0;
        }
        else
        {
            currentIndex = arraySize ? arraySize - 1 : 0;
        }
    }

    void emplace(BTarget *bTarget)
    {
        array[arraySize].value = bTarget;
        array[arraySize].next = currentIndex;
        currentIndex = arraySize;
        ++arraySize;
    }

    BTarget *getItem()
    {
        if (currentIndex == UINT32_MAX)
        {
            return nullptr;
        }

        BTarget *bTarget = array[currentIndex].value;
        currentIndex = array[currentIndex].next;
        return bTarget;
    }
};

class Builder
{
  public:
    BTargetsArrayList updateBTargets;
    uint32_t updateBTargetsSizeGoal = 0;

    mutex executeMutex;
    std::condition_variable cond;

    unsigned short threadCount = 0;
    unsigned short numberOfLaunchedThreads = 0;
    unsigned short numberOfSleepingThreads = 0;
    inline static unsigned short round = 0;
    const unsigned short roundGoal = bsMode == BSMode::BUILD ? 0 : 2;

    bool updateBTargetFailed = false;

    bool errorHappenedInRoundMode = false;

  private:
    bool returnAfterWakeup = false;

  public:
    explicit Builder();
    void execute();
    void addNewTopBeUpdatedTargets(BTarget *bTarget);
    void incrementNumberOfSleepingThreads();
    void decrementNumberOfSleepingThreads();
};

#endif // HMAKE_BUILDER_HPP
