/// \file
/// Defines the Builder class

#ifndef HMAKE_BUILDER_HPP
#define HMAKE_BUILDER_HPP

#include "BTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "PointerArrayList.hpp"
#include "RunCommand.hpp"

#include <condition_variable>
#include <list>
#include <stack>

using std::vector, std::list, std::stack;

/// Current mode of build algorithm
enum class ExecuteMode
{
    WAIT,
    PARALLEL,
    NODE_CHECK,
};

/// Used only on Windows. Kernel fulfills the buffer memory if it is writing. This buffer is not released and is reused
/// for next event.
struct CompletionKey
{
    /// for holding OVERLAPPED struct
    alignas(8) char overlappedBuffer[32];
    /// This is the buffer to 4kb memory. Once allocated, it is reused.
    char *buffer;
    uint64_t handle;
    BTarget *target;
    bool firstCall = true;
};

/// Simultaneously active file-descriptors array in the event loop. CompletionKey buffer is externally initialized.
#ifdef _WIN32
GLOBAL_VARIABLE(CompletionKey *, eventData)
#else
GLOBAL_VARIABLE(BTarget **, eventData)
#endif

/// Only used on Windows. As we reuse the CompletionKey because it has buffer allocated of size 4096. This way eventData
/// also not grow out-of-bounds from 32k entries allocated in constructGlobals function. We emplace the index of
/// CompletionKey in this vector when it is unregistered. When registerEventData is called again, it pops back the last
/// index to reuse the CompletionKey in the eventData at that index.
/// On Linux eventData vector is assigned using the file-descriptor index which Linux kernel allocates as the minimum
/// available.
GLOBAL_VARIABLE(vector<uint64_t>, unusedKeysIndices)

/// Incremented when we have used a new CompletionKey from eventData.
inline uint32_t currentIndex = 0;

/// Core class that has implementation of the build algorithm.
///
/// Build algorithm runs in 2 rounds.
/// Before the round, dependency relations are specified between BTarget. Then those with 0 dependencies
/// (RealBTarget::dependenciesSize == 0) are added in Builder::updateBTargets list. Then these targets are
/// concluded(Builder::updateBTarget or Builder::launchBTarget). After we conclude a target,
/// RealBTarget::dependenciesSize is decremented from the dependents. If this number is 0, then these RealBTarget are
/// added to the Builder::updateBTargets list. Then we go over the Builder::updateBTargets list again.
///
/// For round1:
/// main calls main2 which calls buildSpecification in which all dependency relations are specified. Then main2 calls
/// configureOrBuild which calls Builder::Builder. This constructor will then call BTarget::sortGraph. Then add in
/// Builder::updateBTargets and launch threads with Builder::execute function. This function will start going over the
/// BTargets and call theirs BTarget::updateBTarget in separate threads to conclude them. Builder variables are updated
/// under Builder::executeMutex.
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
/// zero target build speed by 2x-3x. After this Builder::execute returns and Builder constructor completes the round0.
///
/// In this round, after soring and populating Builder::updateBTargets, the server initializes the serverFd which is
/// epoll_create on Linux and CreateIoCompletionPort on Windows. Then we go over the Builder::updateBTargets and call
/// BTarget::launchBTarget which might launches new processes. In this case event is registered in the event loop. When
/// the event happens(process exits or write to stdout/stderr), BTarget::completeBTarget is called of the BTarget
/// returned from the event. After the BTarget::launchBTarget (when no process is launched) or after the
/// BTarget::completeBTarget has reaped the process, we decrement from the dependent RealBTarget::dependenciesSize and
/// if it is 0, add it in the Builder::updateBTargets list. And then go over this list again (call launchBTarget of
/// these BTargets).
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

    /// A central mutex for the build-system operations. Almost all Builder variables are to be updated under this.
    std::mutex executeMutex;
    std::condition_variable cond;
    ExecuteMode exeMode = ExecuteMode::WAIT;

    inline static unsigned short round = 0;
    const unsigned short roundGoal = bsMode == BSMode::BUILD ? 0 : 1;
    bool errorHappenedInRoundMode = false;

    /// This returns the index in the eventData. This returns the CompletionKey index on Windows and fd itself on Linux.
    uint64_t registerEventData(BTarget *target_, uint64_t fd);
    void unregisterEventDataAtIndex(uint64_t index);
    static ReadDataInfo isRecurrentReadFromEventDataCompleted(uint64_t eventIndex, RunCommand &run);
    uint64_t serverFd;
    uint16_t idleCount = 0;

    uint32_t activeEventCount = 0;

  private:
    unsigned short launchedCount = 0;
    unsigned short checkingCount = 0;
    unsigned short checkedCount = 0;
    uint32_t updatedCount = 0;
    bool returnAfterWakeup = false;
    bool updateBTargetFailed = false;
    vector<Node *> uncheckedNodesCentral;
    vector<span<Node *>> uncheckedNodes;

  public:
    explicit Builder();
    void execute();
    void decrementFromDependents(const RealBTarget &rb);
    string pruneRunOutput();
};

#endif // HMAKE_BUILDER_HPP
