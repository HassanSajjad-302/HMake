/// \file
/// Defines the Builder class

#ifndef HMAKE_BUILDER_HPP
#define HMAKE_BUILDER_HPP

#include "BTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "PointerArrayList.hpp"

#include <list>
#include <stack>

using std::vector, std::list, std::stack;

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
/// completed(BTarget::completeBTarget or BTarget::isEventRegistered). After we conclude a target,
/// RealBTarget::dependenciesSize is decremented from the dependents. If this number is 0, then these RealBTarget are
/// added to the Builder::updateBTargets list. Then we go over the Builder::updateBTargets list again.
///
/// For round1:
/// main calls main2 which calls buildSpecification in which all dependency relations are specified. Then main2 calls
/// configureOrBuild which calls Builder::Builder. This constructor will then call BTarget::sortGraph. Then add in
/// Builder::updateBTargets list and then call Builder::execute function. This function will start going over the
/// BTargets and call theirs BTarget::completeRoundOne to conclude them.
///
/// Build-System exits after round1 in BuildMode::CONFIGURE.
///
/// Then Builder::Builder call Builder::nodeCheck function which calls the Node::performNodeCheck of interesting nodes
/// in parallel on Windows and using io-uring if available. After this Builder::round is decremented and round0  is
/// performed.
///
/// In BuildMode::BUILD,
/// For round0:
///
/// Lots of round0 dependency relations are already specified in buildSpecification and during the
/// BTarget::completeRoundOne of round1.
/// In this round, after soring and populating Builder::updateBTargets, the server initializes the serverFd which is
/// epoll_create on Linux and CreateIoCompletionPort on Windows. Then we go over the Builder::updateBTargets and call
/// BTarget::isEventRegistered which might launch new process. In this case event is registered in the event loop.
/// When the event happens(process exits or has message for the build-system), BTarget::isEventCompleted is called of
/// the BTarget returned from the event. After the process exits, we reap the process using RunCommand::reapProcess.
/// After the BTarget::isEventRegistered (when no process is launched) or after reaping the process, we decrement from
/// the dependent RealBTarget::dependenciesSize and if it is 0, add it in the Builder::updateBTargets list. And then go
/// over this list again (call isEventRegistered of these BTargets).
class Builder
{
  public:
    /// Contains those with RealBTarget::dependenciesSize == 0.
    PointerArrayList<RealBTarget> updateBTargets;

    /// In round0, we can not exit once the Builder::updateBTargets::getItem returns nullptr. Even though the list is
    /// empty, the targets whose BTarget::isEventCompleted is being called can add new in this list after completion. We
    /// exit from ExecuteMode::WAIT only when (Builder::updateBTargets.getItem() == nullptr && Builder::updatedCount ==
    /// Builder::updateBTargetsSizeGoal).
    uint32_t updateBTargetsSizeGoal = 0;

    inline static unsigned short round = 0;
    bool errorHappenedInRoundMode = false;

    /// This returns the index in the eventData. This returns the CompletionKey index on Windows and fd itself on Linux.
    /// With help of eventData array, this is used to determine the BTarget whose child process has an event.
    uint64_t registerEventData(BTarget *target_, uint64_t fd);

    bool callIsEventCompleted(BTarget *bTarget, uint64_t index);
    void unregisterEventDataAtIndex(uint64_t index);
    uint64_t serverFd;
    uint16_t idleCount = 0;

    uint32_t activeEventCount = 0;

  private:
    unsigned short launchedCount = 0;
    unsigned short checkingCount = 0;
    unsigned short checkedCount = 0;
    uint32_t updatedCount = 0;
    bool updateBTargetFailed = false;
    vector<Node *> uncheckedNodesCentral;
    vector<span<Node *>> uncheckedNodes;

  public:
    explicit Builder();
    void executeRoundOne();
    void executeRoundZero();
    void checkNodes();
    void execute();
    void decrementFromDependents(const RealBTarget &rb);
};

#endif // HMAKE_BUILDER_HPP
