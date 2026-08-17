/// \file
/// Defines `Builder`, which schedules the build dependency graph.

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

/// Maps OS events back to the target whose process produced them.
#ifdef _WIN32
GLOBAL_VARIABLE(CompletionKey *, eventData)
#else
GLOBAL_VARIABLE(vector<BTarget *>, eventData)
#endif

#ifdef _WIN32
GLOBAL_VARIABLE(vector<uint64_t>, unusedKeysIndices)
#else
GLOBAL_VARIABLE(vector<string *>, freeOutputStrings)
#endif

/// Next unused slot in `eventData` (Windows only).
inline uint32_t currentIndex = 0;

/// Builder runs two passes:
///
/// 1. Round 1 configures targets and creates the dependency graph. It does not launch build processes.
/// 2. Round 0 builds that graph. It starts ready targets, waits for process events, and starts newly ready targets.
///
/// A target is ready when `dependenciesSize` is zero. When a target finishes,
/// `decrementFromDependents()` marks it complete and decreases this count for its dependents. A dependent joins
/// `readyBTargets` when its last required dependency finishes.
///
/// In build mode, `checkNodes()` also records the state of input files before building and any files discovered during
/// the build. This lets the build cache detect content and dependency changes on the next run.
class Builder
{
  public:
    /// Targets ready to run. A waiting C++ module may move one of its ready dependencies to the front.
    PointerArrayList<RealBTarget> readyBTargets;

    /// Number of targets expected to finish in round 0.
    uint32_t readyBTargetsSizeGoal = 0;

    /// Preferred limit for concurrent child processes.
    inline static uint32_t maxSimultaneousProcessDesired = 0;
    /// Current pass: `1` configures the graph; `0` builds it.
    inline static unsigned short round = 0;
    /// Set if a target fails in the current pass.
    bool errorHappenedInRoundMode = false;

    /// Number of child processes currently running.
    uint32_t simultaneousProcessCount = 0;

    /// Registers a target's process output with the platform event loop.
    /// \return event index (`CompletionKey` slot on Windows, fd on Linux).
    uint64_t registerEventData(BTarget *target_, uint64_t fd);

    /// Delivers available output to a target.
    /// \return `true` if the target is still running; `false` if it has finished.
    bool callIsEventCompleted(BTarget *bTarget, uint64_t index);

    /// Removes a completed process from the event loop.
    void unregisterEventDataAtIndex(uint64_t index);

    /// Platform event-loop handle (`epoll` on Linux, IOCP on Windows).
    uint64_t serverFd = static_cast<uint64_t>(-1);

    /// Remaining process slots.
    uint16_t availableProcessSlots = 0;

    /// Number of targets completed in the current pass.
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

    /// Configures the graph without launching child processes.
    void executeRoundOne();

    /// Builds all targets using the asynchronous scheduler.
    void executeRoundZero();

    /// Stats and hashes files in parallel. The final pass includes files discovered while building.
    static void checkNodes();

    /// Runs ready round-1 targets.
    void execute();

    /// Marks a target complete and releases dependents that are now ready.
    void decrementFromDependents(RealBTarget &rb);
};

#endif // HMAKE_BUILDER_HPP
