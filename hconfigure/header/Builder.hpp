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

enum class ExecuteMode
{
    GENERAL,
    NODE_CHECK,
};

class Builder
{
  public:
    PointerArrayList<RealBTarget> updateBTargets;
    uint32_t updateBTargetsCompleted = 0;
    uint32_t updateBTargetsSizeGoal = 0;
    mutex executeMutex;
    std::condition_variable cond;
    ExecuteMode exeMode = ExecuteMode::GENERAL;

    unsigned short threadCount = 0;
    unsigned short launchedCount = 0;
    unsigned short sleepingCount = 0;
    unsigned short checkingCount = 0;
    unsigned short checkedCount = 0;
    inline static unsigned short round = 0;
    const unsigned short roundGoal = bsMode == BSMode::BUILD ? 0 : 1;

    bool updateBTargetFailed = false;

    bool errorHappenedInRoundMode = false;

  private:
    bool returnAfterWakeup = false;
    vector<Node *> uncheckedNodesCentral;
    vector<span<Node *>> uncheckedNodes;

  public:
    explicit Builder();
    void execute();
    void addNewTopBeUpdatedTargets(const RealBTarget *rb);
    void incrementNumberOfSleepingThreads();
    void decrementNumberOfSleepingThreads();
};

#endif // HMAKE_BUILDER_HPP
