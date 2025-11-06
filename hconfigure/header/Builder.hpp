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
    WAIT,
    PARALLEL,
    NODE_CHECK,
};

/// Core class that has implementation of the build algorithm.
///
/// Build algorithm runs in 2 rounds.
/// Before the round, dependency relations are specified between BTarget. Then those with 0 dependencies
/// (RealBTarget::dependenciesSize == 0) are added in Builder::updateBTargets list. Builder::execute then calls
/// BTarget::updateBTarget of these in parallel. After call completion, Builder::executeMutex is locked.
/// RealBTarget::dependenciesSize is decremented from the dependents. If this number is 0, then these RealBTarget are
/// added to the Builder::updateBTargets list. Then Builder::execute unlocks Builder::executeMutex and go over the
/// Builder::updateBTargets list again.
///
/// For round1:
/// main calls main2 which calls buildSpecification in which all dependency relations are specified. Then main2 calls
/// configureOrBuild which calls Builder::Builder. This constructor will then call BTarget::sortGraph. Then add in
/// Builder::updateBTargets and launch threads with Builder::execute function.
///
/// Build-System exits after round1 in BuildMode::CONFIGURE
///
/// In BuildMode::BUILD,
/// For round0:
/// Lots of round0 dependency relations are already specified in buildSpecification and during the
/// BTarget::updateBTarget of round1. However, some of these are partial which are completed in single-thread in
/// BTarget::postRoundOneCompletion. After this Builder::execute switches to ExecuteMode::NODE_CHECK. In this mode, all
/// launched threads call Node::performSystemCheck for selected Nodes in parallel. Node::performSystemCheck is a slow
/// operation and build-system spends 90% of time on this in zero target build. Doing it in multi-thread improves the
/// zero target build speed by 2x-3x. After this Builder::execute switch back to ExecuteMode::WAIT and round0 is
/// completed.
///
///
class Builder
{
  public:
    /// Contains those with RealBTarget::dependenciesSize == 0.
    PointerArrayList<RealBTarget> updateBTargets;
    /// We can not exit once the Builder::updateBTargets::getItem returns nullptr. Even though the list is empty, the
    /// targets whose BTarget::updateBTarget is being called can add new in this list after completion. We exit from
    /// ExecuteMode::WAIT only when (Builder::updateBTargets.getItem() == nullptr &&
    /// Builder::updateBTargets.size() == Builder::updateBTargetsSizeGoal). This is initialized before starting the
    /// ExecuteMode::WAIT and should be incremented when a new BTarget::updateBTarget is to be called.
    uint32_t updateBTargetsSizeGoal = 0;

    /// A central mutex for the build-system operations. Almost all Builder variables are to be udpated under this.
    mutex executeMutex;
    std::condition_variable cond;
    ExecuteMode exeMode = ExecuteMode::WAIT;

    inline static unsigned short round = 0;
    const unsigned short roundGoal = bsMode == BSMode::BUILD ? 0 : 1;
    bool errorHappenedInRoundMode = false;

  private:
    unsigned short launchedCount = 0;
    unsigned short sleepingCount = 0;
    unsigned short checkingCount = 0;
    unsigned short checkedCount = 0;
    bool returnAfterWakeup = false;
    bool updateBTargetFailed = false;
    vector<Node *> uncheckedNodesCentral;
    vector<span<Node *>> uncheckedNodes;

  public:
    explicit Builder();
    void execute();
    void decrementFromDependents(const RealBTarget *rb);
};

#endif // HMAKE_BUILDER_HPP
