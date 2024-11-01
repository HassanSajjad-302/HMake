#ifndef HMAKE_BUILDER_HPP
#define HMAKE_BUILDER_HPP
#ifdef USE_HEADER_UNITS
import "CppSourceTarget.hpp";
import <condition_variable>;
import <vector>;
import <list>;
#else
#include "CppSourceTarget.hpp"
#include <condition_variable>
#include <list>
#include <vector>
#endif

using std::vector, std::list;

class Builder
{
  public:
    list<BTarget *> updateBTargets;
    size_t updateBTargetsSizeGoal = 0;

    list<BTarget *>::iterator updateBTargetsIterator;

    mutex executeMutex;
    std::condition_variable cond;

    unsigned short threadCount = 0;
    unsigned short numberOfLaunchedThreads = 0;
    atomic<unsigned short> numberOfSleepingThreads = 0;
    unsigned short round = 0;
    unsigned short roundGoal = 0;

    bool updateBTargetFailed = false;

  private:
    bool returnAfterWakeup = false;
    bool errorHappenedInRoundMode = false;

  public:
    explicit Builder();
    void execute();
    void incrementNumberOfSleepingThreads();
    void decrementNumberOfSleepingThreads();
    void runEndOfRoundTargets();
};

#endif // HMAKE_BUILDER_HPP
