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
    string buffer;
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

GLOBAL_VARIABLE(string *, processOutputs)
/// Linux-only free-list of reusable `processOutputs` indices.
GLOBAL_VARIABLE(vector<uint64_t>, unusedOutputIndices)

/// Next unused slot in `processOutputs` indices.
inline uint32_t currentIndexOutput = 0;

/// Schedules and executes the dependency graph in two rounds: round 1 first, then round 0 (build mode only).
///
/// Startup (see `initializeCache()` / `configureOrBuild()` in `BuildSystemFunctions.cpp`):
/// - `initializeCache()` loads `nodes` (paths only — stat/hash deferred), `config-cache`, and `build-cache` into
///   `bTargetCaches` before `buildSpecification()` constructs live `BTarget`s.
/// - The `Builder` constructor runs round 1, then (build mode) `checkNodes(true)` in parallel, then round 0.
/// - After the build, `configureOrBuild()` may call `getBuildCache()`, which runs `checkNodes(false)` on any nodes that
///   gained `doHashFile` during the build (e.g. headers discovered while compiling) before rewriting cache entries marked
///   `buildCacheUpdated` / `buildFooterUpdated`.
///
/// Graph and queue (both rounds):
/// - Each `BTarget` owns `realBTargets[0]` and `realBTargets[1]`. Edges are declared with `addDep<round>()` in
///   `buildSpecification()` and may be added while targets run.
/// - `RealBTarget::sortGraph()` topologically sorts the active round; cycles are reported using `getPrintName()`.
/// - `dependenciesSize` is the number of outstanding FULL/WAIT predecessors. When it reaches zero, the bTarget is ready.
/// - Ready bTargets live in `updateBTargets`. `decrementFromDependents()` runs when a bTarget finishes; it decrements
///   dependents and enqueues any that become ready. Round-0 dependency lists are persisted in each target's
///   `BTargetCache::depsCache` when the build cache is written.
///
/// Round 1 — `executeRoundOne()` (synchronous work):
/// - Walks ready bTargets in topological order and calls `BTarget::completeRoundOne()`.
/// - Configure mode stops after this round; `configureOrBuild()` then writes `nodes`, `config-cache`, and `build-cache`.
///
/// Round 0 — `executeRoundZero()` (async processes, `epoll` on Linux / IOCP on Windows):
/// - After `checkNodes(true)`, targets call `setFileStatus()` (see incremental builds below).
/// - `isEventRegistered()` starts work or finishes synchronously; the event loop invokes `isEventCompleted()` on IPC
///   traffic or process exit. Returning `false` completes the bTarget and unblocks dependents.
/// - `CppMod` bring-to-front: if a module/hu is already in `updateBTargets` but `isEventRegistered()` has not run, and
///   compilations are blocked on it, the consumer nulls the old slot at `insertionIndex` and re-enqueues the dependency
///   at the head. That prioritizes work with known waiters and lowers peak memory (fewer idle compiler processes).
///
/// Incremental builds: `checkNodes()` fills `Node::contentHash` (rapidhash of file contents). `setFileStatus()` compares
/// those hashes plus cached `cumulativeHash` / `launchTime` in the build-cache footer — not file mtimes alone.
class Builder
{
  public:
    /// Ready queue for nodes with `dependenciesSize == 0`. `CppMod` can promote a blocked-on dependency to the front
    /// via `RealBTarget::insertionIndex` (see that field).
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
    /// Optional chunked view of unchecked nodes.
    vector<span<Node *>> uncheckedNodes;

  public:
    explicit Builder();
    /// Executes round 1 setup and synchronous scheduler loop.
    void executeRoundOne();
    /// Executes round 0 setup and async event loop.
    void executeRoundZero();
    /// Parallel stat (`doStatFile`) and content-hash (`doHashFile`) pass over `nodeIndices`.
    /// `isFirstTime == true` at round-0 start (all flagged nodes); `false` from `getBuildCache()` before persisting
    /// nodes that were marked during the build (e.g. dynamically discovered headers).
    static void checkNodes(bool isFirstTime);
    /// Core round-1 loop over `updateBTargets`.
    void execute();
    /// Propagates completion to dependents and enqueues newly ready nodes.
    void decrementFromDependents(RealBTarget &rb);
    /// Returns how many new processes may be launched now.
    uint32_t getCapacityForNewProcesses() const;
};

#endif // HMAKE_BUILDER_HPP
