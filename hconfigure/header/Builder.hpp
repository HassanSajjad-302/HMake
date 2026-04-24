/// \file
/// Defines `Builder`, the core graph scheduler/executor.

#ifndef HMAKE_BUILDER_HPP
#define HMAKE_BUILDER_HPP

#include "BTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "PointerArrayList.hpp"

#include <list>
#include <stack>

using std::vector, std::list, std::stack;

/// Windows-only event payload kept in `eventData`.
/// The read buffer is reused across completions.
struct CompletionKey
{
    /// Storage for an `OVERLAPPED` object.
    alignas(8) char overlappedBuffer[32];
    /// Reusable 4 KiB read buffer.
    char *buffer;
    uint64_t handle;
    BTarget *target;
};

/// Active event-index table used by the event loop.
#ifdef _WIN32
GLOBAL_VARIABLE(CompletionKey *, eventData)
#else
GLOBAL_VARIABLE(BTarget **, eventData)
#endif

/// Windows-only free-list of reusable `eventData` indices.
/// Linux maps file descriptors directly and does not use this pool.
/// On Windows, HANDLE are not incremental.
GLOBAL_VARIABLE(vector<uint64_t>, unusedKeysIndices)

/// Next unused slot in `eventData` (Windows only).
inline uint32_t currentIndex = 0;

GLOBAL_VARIABLE(string*, processOutputs)
/// Linux-only free-list of reusable `processOutputs` indices.
GLOBAL_VARIABLE(vector<uint64_t>, unusedOutputIndices)

/// Next unused slot in `processOutputs` indices.
inline uint32_t currentIndexOutput = 0;

/// Implements HMake scheduling and execution across both rounds.
///
/// High-level flow:
/// 1. Build dependency graph for a round. It is built in `buildSpecification` function and during the BTarget
/// execution.
/// 2. Seed `updateBTargets` with ready RealBTarget.Those whose `dependenciesSize == 0`.
/// 3. Execute each of these RealBTarget. By calling `BTarget::completeRoundOne` in round1 and,
/// `BTarget::isEventRegistered` and then maybe `BTarget::isEventCompleted` for round0.
/// 4. Decrement unresolved dependency counters of dependents. That is `RealBTarget::dependenciesSize`.
/// 5. Enqueue newly ready dependents and continue until complete.
///
/// Round 1:
/// - Executes synchronous work via `BTarget::completeRoundOne`.
/// - In configure mode, execution ends after this round.
///
/// Round 0 (build mode only):
/// - Performs async work using an event loop (`epoll` on Linux, IOCP on Windows).
/// - Calls `BTarget::isEventRegistered` to start/attach work.
/// - Calls `BTarget::isEventCompleted` on process/message events.
/// - Marks targets complete when one of these functions return false.
class Builder
{
  public:
    /// Ready queue containing nodes with `dependenciesSize == 0`.
    PointerArrayList<RealBTarget> updateBTargets;

    /// Round-0 termination goal: number of nodes expected to be consumed.
    /// Execution stops only when `updatedCount == updateBTargetsSizeGoal`.
    uint32_t updateBTargetsSizeGoal = 0;

    /// Global cap derived from hardware for concurrent process pressure.
    inline static uint32_t maxSimultaneousProcessDesired = 0;
    /// Active round (`1` then `0`).
    inline static unsigned short round = 0;
    /// Set when any node fails during current round.
    bool errorHappenedInRoundMode = false;

    /// Number of currently running child processes.
    uint32_t simultaneousProcessCount = 0;

    /// Registers an async source in `eventData`.
    /// \return event index (`CompletionKey` slot on Windows, fd on Linux).
    uint64_t registerEventData(BTarget *target_, uint64_t fd);

    /// Internal function that calls `BTarget::isEventCompleted` when an event is received.
    /// \return true if target is still waiting on events; false if target is complete.
    bool callIsEventCompleted(BTarget *bTarget, uint64_t index);

    /// Removes event registered at `index`.
    void unregisterEventDataAtIndex(uint64_t index);

    /// Multiplexing handle (`epoll` fd or IOCP handle).
    uint64_t serverFd;

    /// Available launcher slots.
    uint16_t idleCount = 0;

    /// Count of targets currently registered in event loop. Used for debugging purposes.
    uint32_t activeEventCount = 0;

    /// Number of targets already consumed from `updatedBTarget` queue.
    uint32_t updatedCount = 0;

  private:
    /// Internal counters used by node-check code path.
    unsigned short launchedCount = 0;
    unsigned short checkingCount = 0;
    unsigned short checkedCount = 0;
    bool updateBTargetFailed = false;
    /// Flattened list of nodes that requested a filesystem refresh.
    vector<Node *> uncheckedNodesCentral;
    /// Optional chunked view of unchecked nodes.
    vector<span<Node *>> uncheckedNodes;

  public:
    explicit Builder();
    /// Executes round 1 setup and synchronous scheduler loop.
    void executeRoundOne();
    /// Executes round 0 setup and async event loop.
    void executeRoundZero();
    /// Performs pending `Node::performSystemCheck()` calls.
    void checkNodes();
    /// Core round-1 loop over `updateBTargets`.
    void execute();
    /// Propagates completion to dependents and enqueues newly ready nodes.
    void decrementFromDependents(const RealBTarget &rb);
    /// Returns how many new processes may be launched now.
    uint32_t getCapacityForNewProcesses() const;
};

#endif // HMAKE_BUILDER_HPP
