/// \file
/// Defines the Builder class

#ifndef HMAKE_BUILDER_HPP
#define HMAKE_BUILDER_HPP

#include "BTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "PointerArrayList.hpp"
#include <condition_variable>
#include <list>
#include <vector>

using std::vector, std::list;

/// Current mode of build algorithm
enum class ExecuteMode
{
    GENERAL,
    NODE_CHECK,
};

/// Core class that has implementation of the build algorithm.
///
/// Build algorithm runs in 2 rounds.
/// For every round, first of all dependency relations are specified between BTarget. Then those with 0 dependencies
/// (RealBTarget::dependenciesSize == 0) are added in Builder::updateBTarget list. Builder::execute then calls
/// BTarget::updateBTarget of these in parallel. After call completion, Builder::executeMutex is locked.
/// RealBTarget::dependenciesSize is decremented from the dependents. If this number is 0, then these RealBTarget is
/// added to the Builder::updateBTargets list.
///
/// For round1:
/// main calls main2 which calls buildSpecification in which all dependency relations are specified. Then main2 calls
/// configureOrBuild which calls Builder::Builder. This constructor will then call BTarget::sortGraph. Then add in
/// Builder::updateBTargets and launch threads with Builder::execute function.
///
/// For round0:
/// Lots of round0 dependency relations are already specified in buildSpecification and during the
/// BTarget::updateBTarget of round1. However, some of these are partial which are completed in single-thread in
/// BTarget::postRoundOneCompletion. After this Builder::execute switches to ExecuteMode::NODE_CHECK. In this mode, all
/// launched threads call Node::performSystemCheck for selected Nodes in parallel. Node::performSystemCheck is a slow
/// operation and build-system spends 90% of time on this. Doing it in multi-thread improves the zero target build speed
/// by 2x-3x. After this Builder::execute switch back to ExecuteMode::GENERAL and round0 is completed.
///
///
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
    void decrementFromDependents(const RealBTarget *rb);
    void incrementNumberOfSleepingThreads();
    void decrementNumberOfSleepingThreads();
};

#endif // HMAKE_BUILDER_HPP
