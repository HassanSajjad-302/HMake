/// \file
/// Defines `BTarget` and `RealBTarget`, the core scheduling units of HMake.

#ifndef HMAKE_BASICTARGETS_HPP
#define HMAKE_BASICTARGETS_HPP

#include "Node.hpp"
#include "PointerPacking.h"
#include "RunCommand.hpp"
#include "gtl/include/gtl/phmap.hpp"
#include <array>
#include <memory_resource>
#include <span>
#include <string>
#include <vector>

using std::size_t, std::vector, gtl::flat_hash_map, gtl::flat_hash_set, std::lock_guard, std::array, std::string,
    std::string_view, gtl::btree_set;

class BTarget;

/// Compares `BTarget` pointers by round-0 topological order.
struct IndexInTopologicalSortComparatorRoundZero
{
    bool operator()(const BTarget *lhs, const BTarget *rhs) const;
};

/// Compares `BTarget` pointers by round-1 topological order.
struct IndexInTopologicalSortComparatorRoundTwo
{
    bool operator()(const BTarget *lhs, const BTarget *rhs) const;
};

/// Dependency relation between two `RealBTarget` nodes.
enum class RelationType : uint8_t
{
    /// The dependent waits for completion.
    /// In round 0, `selectiveBuild` propagates from dependent to dependency.
    FULL = 0,

    /// The dependent waits for completion, but does not propagate `selectiveBuild`.
    /// Currently unused.
    WAIT = 1,

    /// The dependent does not wait, but `selectiveBuild` will be propagated. Used in specifying CppTarget dep with the
    /// other CppTarget as we want the dependency CppTarget's huDeps and imodDeps to be built.
    SELECTIVE = 2,

    /// Order-only relation used for graph ordering. Used to specify stat-lib dependency with other static-lib.
    LOOSE = 3,
};

class RealBTarget;

/// Runtime classifier for derived target kinds.
/// 4 bits allocated (bits 3-6 of the packed pointer-int pair) — supports up to 16 values.
enum class BTargetType : uint8_t
{
    UNKNOWN = 0,
    CPP_MOD = 1,
    LOAT = 2,
    CPP_TARGET = 3,
    CPP_SRC = 4,
    CONFIGURATION = 5,
    PLOAT = 6,
    // 7–15 reserved for future expansion
};

// we use true as there is no need for null check
#define FOR_DEPS(container_, round_, btype_, type_, var_)                                                              \
    for (const RBTWithType &_rbt_##var_ : (container_).realBTargets[(round_)].dependencies)                            \
        if (_rbt_##var_.getAbstractType() == (btype_))                                                                 \
            if (auto *var_ = static_cast<type_ *>(_rbt_##var_.getPointer()->getBTarget()); true)

/// RealBTargetWithType
/// Layout: single 8-byte word.
///   bits 0-2  : RelationType  (3 bits, 5 values)
///   bits 3-6  : BTargetType     (4 bits, 16 values)
///   bits 7-63 : RealBTarget*    (requires alignas(128) on RealBTarget)
struct RBTWithType
{
    PointerIntPair<RealBTarget *, 7, uint8_t> ptrAndBoth;

    static constexpr uintptr_t kRelationTypeMask = uintptr_t(0x07); // bits 0-2
    static constexpr uintptr_t kBTargetTypeMask = uintptr_t(0x78);  // bits 3-6
    static constexpr uintptr_t kMetadataMask = uintptr_t(0x7F);     // bits 0-6
    static constexpr uintptr_t kAbstractTypeMax = uintptr_t(0x0F);  // 4 bits

    static constexpr uint8_t pack(RelationType depKind, BTargetType type)
    {
        return static_cast<uint8_t>(type) << 3 | static_cast<uint8_t>(depKind);
    }

    RBTWithType(RealBTarget *ptr, const RelationType depKind, const BTargetType type)
        : ptrAndBoth(ptr, pack(depKind, type))
    {
    }

    RealBTarget *getPointer() const
    {
        return ptrAndBoth.getPointer();
    }

    RelationType getRelationType() const
    {
        return static_cast<RelationType>(ptrAndBoth.getRaw() & kRelationTypeMask);
    }

    BTargetType getAbstractType() const
    {
        return static_cast<BTargetType>(ptrAndBoth.getRaw() >> 3 & kAbstractTypeMax);
    }

    void setRelationType(RelationType depKind)
    {
        ptrAndBoth =
            decltype(ptrAndBoth)::fromRaw(ptrAndBoth.getRaw() & ~kRelationTypeMask | static_cast<uintptr_t>(depKind));
    }

    void setAbstractType(BTargetType type)
    {
        ptrAndBoth =
            decltype(ptrAndBoth)::fromRaw(ptrAndBoth.getRaw() & ~kBTargetTypeMask | static_cast<uintptr_t>(type) << 3);
    }

    void setMetadata(const RelationType depKind, const BTargetType type)
    {
        ptrAndBoth = decltype(ptrAndBoth)::fromRaw(ptrAndBoth.getRaw() & ~kMetadataMask |
                                                   static_cast<uintptr_t>(pack(depKind, type)));
    }
};

static_assert(sizeof(RBTWithType) == 8, "RBTWithType must be 8 bytes — check RealBTarget alignment");

struct RBTDepTypeHash
{
    using is_transparent = void;

    size_t operator()(const RBTWithType &e) const
    {
        // RealBTarget is alignas(128) so low 7 bits are always zero — shift them out for better distribution
        return reinterpret_cast<size_t>(e.getPointer()) >> 7;
    }

    size_t operator()(RealBTarget *ptr) const
    {
        return reinterpret_cast<size_t>(ptr) >> 7;
    }
};

struct RBTDepTypeEqual
{
    using is_transparent = void;

    bool operator()(const RBTWithType &a, const RBTWithType &b) const
    {
        return a.getPointer() == b.getPointer();
    }

    bool operator()(const RBTWithType &a, RealBTarget *ptr) const
    {
        return a.getPointer() == ptr;
    }

    bool operator()(RealBTarget *ptr, const RBTWithType &a) const
    {
        return a.getPointer() == ptr;
    }
};

using DepSet = flat_hash_set<RBTWithType, RBTDepTypeHash, RBTDepTypeEqual>;

/// Incremental-build decision for a `RealBTarget` in round 0.
enum class UpdateStatus : char
{
    UNCHECKED = 0,         ///< Not yet evaluated by `setFileStatus()`.
    UPDATE_NEEDED = 1,     ///< Inputs or dependencies changed; this target must run.
    UPDATE_NOT_NEEDED = 2, ///< Up to date; skip launching work.
};

/// Every BTarget has 2 of these so distinct dependency order can be specified for the 2 rounds.
class alignas(128) RealBTarget
{
    /// Contains RealBTarget* that form the cycle if there is any.
    inline static vector<RealBTarget *> cycle;

    /// Set by `sortGraph()` when the dependency graph is cyclic.
    inline static bool cycleExists = false;

    /// DFS helper to report one concrete cycle path.
    static bool findCycleDFS(RealBTarget *node, flat_hash_set<RealBTarget *> &visited,
                             flat_hash_set<RealBTarget *> &recursionStack, vector<RealBTarget *> &currentPath,
                             string &errorString);

  public:
    /// Topologically sorted nodes produced by `sortGraph()`.
    inline static vector<RealBTarget *> sorted;

    /// Graph view consumed by `sortGraph()`.
    inline static std::span<RealBTarget *> graphEdges;

    /// Topologically sorts `graphEdges`; exits with an error when a cycle exists.
    static void sortGraph();

    /// Debug utility that prints `sorted` in order.
    static void printSortedGraph();

    /// Reverse edges: "who depends on me".
    DepSet dependents;
    /// Forward edges: "what I depend on".
    DepSet dependencies;

    /// Content fingerprint for incremental builds when `BTarget::launchesProcess` is true.
    ///
    /// Subclasses (e.g. `CppSrc`, `CppMod`, `HeaderGen`) compute this in `setFileStatus()` — typically a rapidhash over
    /// `commandHash` plus `Node::contentHash` values from `checkNodes()` — then call `BTarget::setFileStatus()`, which
    /// compares the live value to the footer stored in `BTargetCache` (loaded by `readBuildCache()` / updated at end of
    /// build via `getBuildCache()`). A mismatch sets `updateStatus` to `UpdateStatus::UPDATE_NEEDED`.
    uint64_t cumulativeHash = 0;

    /// Timestamp of the last successful launch/build, in nanoseconds since the Unix epoch.
    ///
    /// When `BTarget::launchesProcess` is true:
    ///   - Set to the wall-clock time immediately before the child process is started.
    ///   - Restored from the build-cache footer in `initializeBTarget()`.
    ///   - `BTarget::setFileStatus()` marks the target stale if any FULL/WAIT dependency has a greater `launchTime`
    ///     (a dependency was rebuilt after this target last ran).
    ///
    /// When `BTarget::launchesProcess` is false:
    ///   - Not read from the build-cache footer.
    ///   - After recursively evaluating dependencies, `BTarget::setFileStatus()` sets this to the maximum
    ///     `launchTime` among those dependencies so upstream targets can detect downstream rebuilds.
    uint64_t launchTime = -1;

    /// Once sorted the index of this RealBTarget in the topological sorted array. Used in sorting to provide static
    /// libs in order as some linkers have this requirement.
    uint32_t indexInTopologicalSort : 29 = 0;

    /// Incremental-build state for round 0. Set by `setFileStatus()` and subclass overrides.
    /// `Builder::decrementFromDependents()` marks this `RealBTarget` completed; if it finished with
    /// `UpdateStatus::UPDATE_NEEDED`, FULL dependents are also marked `UpdateStatus::UPDATE_NEEDED`.
    UpdateStatus updateStatus = UpdateStatus::UNCHECKED;

    /// Count of incoming FULL/WAIT edges. Incremented in `addDep()`; decremented in
    /// `Builder::decrementFromDependents()` when this bTarget completes. When it reaches zero, the dependent is enqueued
    /// in `Builder::updateBTargets`.
    uint32_t dependenciesSize : 30 = 0;

    /// Set to true in `Builder::decrementFromDependents()` after round-0 work for this node finishes.
    bool isCompleted : 1 = false;

    /// Which element of BTarget::realBTargets[2] this instance occupies (0 or 1).
    /// Used by getBTarget() to recover the enclosing BTarget via pointer arithmetic.
    bool round : 1 = 0;

    // TODO
    //  Following describes the time taken for the completion of this task. Currently unused.
    // unsigned long timeTaken = 0;

    /// Which `Builder::updateBTargets` array cell holds this node (set by `PointerArrayList::emplace`).
    ///
    /// `CppMod` bring-to-front: a consumer that is blocked on this module/hu nulls `updateBTargets.array[insertionIndex].value`
    /// and re-enqueues it at the head when it is already in the ready queue but `isEventRegistered` has not run yet
    /// (`getItem()` skips the nulled slot).
    ///
    /// We prioritize work that already has waiters over other ready targets that may have none. Each waiter keeps a
    /// compiler process alive until the dependency completes; starting it sooner ends those waits earlier and reduces peak
    /// memory during the build.
    uint32_t insertionIndex = -1;

    /// Process exit status for round 0 (`EXIT_SUCCESS` or `EXIT_FAILURE`). Set from `RunCommand::exitStatus` when an
    /// async child exits; failures propagate to FULL dependents via `Builder::decrementFromDependents()`.
    bool exitStatus = EXIT_SUCCESS;

    /// \param round_ which slot in BTarget::realBTargets this occupies (0 or 1).
    /// Constructor will add this into `BTarget::realBTargetsGlobal[round]`.
    RealBTarget(unsigned short round_);

    /// \param round which slot in BTarget::realBTargets this occupies (0 or 1).
    /// \param add if false, Constructor will not add this in `BTarget::realBTargetsGlobal[round]`.
    RealBTarget(unsigned short round_, bool add);
    bool checkDepsChanged() const;

    /// Adds dependency edge for a given round and dependency type.
    template <BTargetType bTargetType = BTargetType::UNKNOWN, RelationType depType = RelationType::FULL>
    void addDep(RealBTarget *dep);

    /// Recovers the enclosing BTarget via compile-time offset arithmetic.
    /// Zero-cost: no stored pointer, no virtual dispatch.
    BTarget *getBTarget() const;

    void getAllWaitDeps(flat_hash_set<RBTWithType, RBTDepTypeHash, RBTDepTypeEqual> &allDepsTransitive);
    void getAllWaitDepsTopological(btree_set<BTarget *, IndexInTopologicalSortComparatorRoundZero> &allDepsTransitive);
};

static_assert(alignof(RealBTarget) >= 128,
              "RealBTarget must be 128-byte aligned to pack 7 bits (3 RelationType + 4 BTargetType) into pointer");

/// Base class for all build graph tasks.
///
/// Each instance is tied to persistent cache storage through `cacheIndex`, an index into `bTargetCaches`.
/// `bTargetCaches[cacheIndex]` holds this target's `configCache` blob (configure-time) and build-cache slice (dependency
/// prefix + body + optional 16-byte footer). Subclasses implement `writeConfigCacheAtConfigTime()` /
/// `writeBuildCacheAtConfigTime()` / `writeBuildCacheAtBuildTime()` to fill those buffers; `initializeBTarget()` wires
/// the live `BTarget` pointer after `readConfigCache()` / `readBuildCache()` in `initializeCache()`.
///
/// Lookup key is `cacheName` (`nameToIndexMap`). It must be unique across targets — two `BTarget`s with the same
/// `cacheName` are rejected at configure-time (`checkForSameTargetName()`). Constructors that take only `name_` set
/// `cacheName` to `rapidhash(name_)` (after lower-casing `name`); use the `cacheName_` overload when you need a stable
/// key independent of the display name (e.g. `ObjectFile` with an empty `name`).
///
/// When `launchesProcess` is true, the build-cache entry always includes a footer (`cumulativeHash`, `launchTime`):
/// written at configure-time for new targets and updated after a successful build via `writeBuildCacheHeaderAtBuildTime()`.
///
/// Derived classes override execution hooks:
/// - `completeRoundOne()` for synchronous round-1 work
/// - `isEventRegistered()` / `isEventCompleted()` for event-driven round-0 work
///
/// Ordering constraints are declared with `addDep`.
class BTarget // BTarget
{
    friend RealBTarget;

  public:
    /// Per-round global storage of all `RealBTarget` nodes registered at construction (`add == true`).
    /// Indexed by `RealBTarget::round` (0 or 1); consumed by `RealBTarget::sortGraph()`.
    inline static array<std::span<RealBTarget *>, 2> realBTargetsGlobal;

    /// Number of entries in `realBTargetsGlobal[round]` for each round.
    inline static array<uint32_t, 2> realBTargetsArrayCount{};

    /// Process/event helper used by round-0 async workflows (`startAsyncProcess`, IPC pipes, captured output).
    RunCommand run;

  private:
    friend class Builder;
    friend void constructGlobals();

  public:
    /// Monotonically increasing count of constructed `BTarget` instances (used to assign `id`).
    inline static uint32_t total = 0;

    /// Human-readable target name (lower-cased). Used in logs, selective-build matching, and cycle diagnostics.
    string name;

    /// Unique runtime id assigned at construction (`++total`).
    size_t id = 0;

    /// Index into `bTargetCaches` / `nameToIndexMap` for this target's persisted cache slice.
    uint32_t cacheIndex = -1;

    /// Set when this target's build-cache body changed; `getBuildCache()` calls `writeBuildCacheAtBuildTime()`.
    bool buildCacheUpdated = false;

    /// Set when the 16-byte footer (`cumulativeHash` + `launchTime`) changed; `getBuildCache()` calls
    /// `writeBuildCacheHeaderAtBuildTime()`. If neither this nor `buildCacheUpdated` is set, the prior blob is reused.
    bool buildFooterUpdated = false;

    /// Unique key for `nameToIndexMap` and `bTargetCaches[cacheIndex].name`. From `rapidhash(name)` or an explicit
    /// constructor argument; must not collide with another target's `cacheName`.
    uint64_t cacheName;

    /// Runtime type tag supplied at construction — replaces virtual getBTargetType().
    BTargetType bTargetType = BTargetType::UNKNOWN;

    // TODO
    // Following describes total time taken across all rounds. i.e. sum of all RealBTarget::timeTaken.
    // float totalTimeTaken = 0.0f;

    /// Whether this target will launch a child process. Supplied at construction.
    bool launchesProcess = false;

    /// Whether this target is selected for build in the current invocation.
    /// Computed by `setSelectiveBuild()`.
    bool selectiveBuild = false;

    /// If true, this target is selected only when explicitly requested on CLI.
    bool buildExplicit = false;

    /// True when this target had no config-cache entry on startup and a new `BTargetCache` slot was created.
    bool newlyAdded = false;

    /// Round 0 (async/event-driven) and round 1 (synchronous) dependency graphs for this target.
    array<RealBTarget, 2> realBTargets;

    /// Registers this target in `bTargetCaches` and links `bTargetCaches[cacheIndex].bTarget`.
    ///
    /// `BSMode::CONFIGURE`: if `makeDirectory`, creates `configureDir/name`. Looks up `cacheName` in `nameToIndexMap`;
    /// allocates a new `BTargetCache` slot (and sets `newlyAdded`) when absent, otherwise reuses the existing index.
    /// Enforces unique target names per `cacheName` via `checkForSameTargetName()`.
    ///
    /// `BSMode::BUILD`: requires an existing config-cache entry (errors if `cacheName` is missing — run configure
    /// first). config-cache / build-cache slices were loaded earlier in `initializeCache()` → `readConfigCache()` /
    /// `readBuildCache()`. When `launchesProcess` is true, restores `realBTargets[0].launchTime` from the footer;
    /// `cumulativeHash` is compared later in `setFileStatus()`.
    ///
    /// TODO: persist `launchesProcess` in config-cache (e.g. packed in `BTargetCache::bTarget`) and error if it
    /// differs from the current constructor argument.
    void initializeBTarget(bool makeDirectory);

    /// \param name_ human-readable name (lower-cased); `cacheName` is set to `rapidhash(name_)`. Must be unique among
    /// targets. If `launchesProcess`, a build-cache footer is stored for this target.
    BTarget(string name_, bool launchesProcess_, BTargetType type_);
    /// \param name_ human-readable name (lower-cased); `cacheName` is `rapidhash(name_)`. Must be unique among targets.
    /// \param makeDirectory if true, HMake creates `configureDir/name` at configure-time.
    /// If `launchesProcess`, a build-cache footer is stored for this target.
    BTarget(string name_, bool launchesProcess_, BTargetType type_, bool buildExplicit_, bool makeDirectory);
    /// \param name_ human-readable name (lower-cased); `cacheName` is `rapidhash(name_)`. Must be unique among targets.
    /// \param add0 / \p add1 if false, that round's `RealBTarget` is not registered in `realBTargetsGlobal`.
    /// If `launchesProcess`, a build-cache footer is stored for this target.
    BTarget(string name_, bool launchesProcess_, BTargetType type_, bool buildExplicit_, bool makeDirectory, bool add0,
            bool add1);

    /// \param name_ display name (lower-cased). \p cacheName_ is the cache key (must be unique; must match configure-time
    /// entry on build). If `launchesProcess`, a build-cache footer is stored for this target.
    BTarget(string name_, uint64_t cacheName_, bool launchesProcess_, BTargetType type_);
    /// \param cacheName_ unique cache key; must match the value used at configure-time on build.
    /// \param makeDirectory if true, HMake creates `configureDir/name` at configure-time.
    /// If `launchesProcess`, a build-cache footer is stored for this target.
    BTarget(string name_, uint64_t cacheName_, bool launchesProcess_, BTargetType type_, bool buildExplicit_,
            bool makeDirectory);
    /// \param cacheName_ unique cache key; must match configure-time on build.
    /// If `launchesProcess`, a build-cache footer is stored for this target.
    BTarget(string name_, uint64_t cacheName_, bool launchesProcess_, BTargetType type_, bool buildExplicit_,
            bool makeDirectory, bool add0, bool add1);

    virtual ~BTarget();
    void writeBuildCacheHeaderAtBuildTime(string &buffer) const;

    /// Might set the BTarget::selectiveBuild variable based on BTarget::name, hbuild execution directory and passed
    /// arguments. Called in round1 before `completeRoundOne` call.
    void setSelectiveBuild();

    /// Returns true when `hbuild` runs in this target directory or one of its children.
    bool isHBuildInSameOrChildDirectory() const;

    /// string is used in logs and cycle diagnostics.
    virtual string getPrintName() const;

    /// Round-1 synchronous work entry point.
    virtual void completeRoundOne();

    /// Round-0 registration hook for async/event-driven work.
    /// \return true if the target registered/waits on an event; false if it completed immediately. Should return true
    /// if run.startAsyncProcess is called.
    virtual bool isEventRegistered(Builder &builder);

    /// This function will be called only if the child process exited, or if it printed on stdout, followed by the
    /// delimiter. In case, the process exited, the following is called after reaping the process and
    /// realBTargets[0].exitStatus is also set to the exitStatus of the process.
    /// \message Child-process message to the build-system. if this is empty, it means that the child-process has
    /// exited, and it has been reaped. In that case, run.output is the output of the child process.
    /// \return If this function returns false, Builder::decrementFromDependents is called. Otherwise, it is assumed
    /// that the BTarget is waiting for further messages.
    virtual bool isEventCompleted(Builder &builder, string_view message);

    /// Determines whether `realBTargets[0]` needs to run. No-op if `updateStatus != UNCHECKED`.
    ///
    /// When `launchesProcess` is true: compares `realBTargets[0].cumulativeHash` against the stored value in the
    /// build-cache footer — a mismatch immediately sets `UPDATE_NEEDED`. Otherwise, `highestTime` starts at
    /// `realBTargets[0].launchTime` (restored from the footer by `initializeBTarget()`).
    ///
    /// When `launchesProcess` is false: `highestTime` starts at 0.
    ///
    /// Recurses over FULL/WAIT dependencies (calling `setFileStatus()` on any that are still `UNCHECKED`). For each:
    /// - If the dependency is `UPDATE_NEEDED`, this target is also `UPDATE_NEEDED`.
    /// - If `launchesProcess` and `depRb->launchTime > highestTime` (a dep was rebuilt after this target last ran),
    ///   this target is `UPDATE_NEEDED`.
    /// - Otherwise `highestTime = max(highestTime, depRb->launchTime)`.
    ///
    /// If no dependency triggers a rebuild, sets `UPDATE_NOT_NEEDED`. When `launchesProcess` is false, propagates
    /// `highestTime` into `realBTargets[0].launchTime` so upstream targets can detect downstream rebuilds.
    ///
    /// Subclasses (`CppSrc`, `CppMod`, `LOAT`, `HeaderGen`) compute `cumulativeHash` from content hashes of inputs
    /// and call this base implementation via `ObjectFile::setFileStatus()` / `PLOAT::setFileStatus()`.
    virtual void setFileStatus();

    /// This function is called in standAlone mode, so the BTarget could generate stand-alone commands that could be
    /// run stand-alone without the need for the build-system.
    virtual void generateStandAloneCommand();

    /// This function is called by dependents of BTarget, so the scriptFile could be populated. This script could then
    /// be run alone without HMake. Used in producing the script for compiling module-file including its dependencies.
    /// This function is not part of CppMod class because CppMod class could have some dependencies that could like to
    /// add to the script
    /// \param dirPath is the directory-path where the response files and the script file is written.
    /// \param direct
    virtual void cppStandAloneCommand(flat_hash_set<string> &createdDirs, string &scriptContents,
                                      const string &scriptDir, bool direct);

    virtual void writeConfigCacheAtConfigTime(string &buffer);
    virtual void writeBuildCacheAtConfigTime(string &buffer)
    {
    }
    virtual void writeBuildCacheAtBuildTime(string &buffer);
    virtual void verifyBuildCache(string_view buildCache) const;
    virtual void verifyConfigCache(string_view configCache) const
    {
    }
    void verifyBTargetHeader(string_view buildCache, uint32_t &bytesRead) const;

    template <unsigned round, BTargetType type = BTargetType::UNKNOWN, RelationType depType = RelationType::FULL>
    void addDep(BTarget *dep);
};
bool operator<(const BTarget &lhs, const BTarget &rhs);

template <BTargetType bTargetType, RelationType relationType> void RealBTarget::addDep(RealBTarget *dep)
{
    if (dependencies.emplace(dep, relationType, bTargetType).second)
    {
        dep->dependents.emplace(this, relationType, bTargetType);
        if constexpr (relationType == RelationType::FULL || relationType == RelationType::WAIT)
        {
            ++dependenciesSize;
        }
    }
}

template <unsigned round, BTargetType type, RelationType relationType> void BTarget::addDep(BTarget *dep)
{
    if (realBTargets[round].dependencies.emplace(&dep->realBTargets[round], relationType, type).second)
    {
        dep->realBTargets[round].dependents.emplace(&realBTargets[round], relationType, type);
        if constexpr (relationType == RelationType::FULL || relationType == RelationType::WAIT)
        {
            ++realBTargets[round].dependenciesSize;
        }
    }
}

/// Recovers the enclosing BTarget using compile-time offset arithmetic.
/// Safe because RealBTarget objects only ever live as members of BTarget::realBTargets[2].
/// round_ (0 or 1) tells us which array element we are, which fixes the back-offset exactly.
inline BTarget *RealBTarget::getBTarget() const
{
    return reinterpret_cast<BTarget *>(reinterpret_cast<char *>(const_cast<RealBTarget *>(this)) -
                                       offsetof(BTarget, realBTargets) - round * sizeof(RealBTarget));
}

/// Per-target slices of the on-disk caches managed by `initializeCache()` / `configureOrBuild()` in
/// `BuildSystemFunctions.cpp`.
///
/// On disk (under the configure directory, optionally LZ4-compressed):
/// - `config-cache` — one entry per target: `cacheName`, sized `configCache` blob (written at configure-time).
/// - `build-cache` — parallel array: inline `depsCache` (round-0 FULL/WAIT `cacheIndex` list), then sized per-target
///   body; process-launching targets append a 16-byte footer (`cumulativeHash`, `launchTime`).
///
/// `readConfigCache()` / `readBuildCache()` fill `bTargetCaches` and `nameToIndexMap` before `buildSpecification()`.
/// Live `BTarget` constructors attach via `initializeBTarget()`. At configure end or after a build,
/// `getConfigCache()` / `getBuildCache()` serialize updates (build mode re-hashes nodes via `checkNodes(false)` when
/// needed, then rewrites only entries with `buildCacheUpdated` / `buildFooterUpdated`).

/// Order of entries is stable across builds; each target has matching config and build slices.
class BTargetCache
{
  public:
    /// Back-pointer to the live `BTarget` owning this cache entry.
    BTarget *bTarget = nullptr;

    /// Same value as `BTarget::cacheName` — hash key for this entry in `nameToIndexMap`.
    uint64_t name;

    /// Target-specific config-cache payload (written at configure-time, read-only at build-time).
    string_view configCache;

    /// Inline prefix in the on-disk build-cache entry: round-0 FULL/WAIT dependency `cacheIndex` list (see `readBuildCache()`).
    string_view depsCache;

  private:
    /// Full build-cache blob: body followed by a fixed 16-byte footer (`cumulativeHash` + `launchTime`).
    string_view buildCache;

  public:
    /// Build-cache body excluding the trailing 16-byte footer.
    string_view getBuildCache() const
    {
        return string_view{buildCache.data(), buildCache.size() - 16};
    }
    /// Full build-cache blob including the footer.
    string_view getFullBuildCache() const
    {
        return buildCache;
    }
    /// Trailing 16 bytes: `cumulativeHash` (8) + `launchTime` (8).
    string_view getBuildFooter() const
    {
        const uint64_t starting = buildCache.size() - 16;
        return string_view{buildCache.data() + starting, 16};
    }
    void setBuildCache(const string_view buildCache_)
    {
        buildCache = buildCache_;
    }
};

inline vector<BTargetCache> bTargetCaches;
inline flat_hash_map<uint64_t, uint32_t> nameToIndexMap;

enum class CppModType : uint8_t
{
    CPP_SRC = 0,                 ///< Ordinary source file (non-module).
    PRIMARY_EXPORT = 1,          ///< Module interface unit exporting the primary module.
    PARTITION_EXPORT = 2,      ///< Module interface unit for a module partition.
    HEADER_UNIT = 3,             ///< Header unit (`.h` compiled as a BMI producer).
    PRIMARY_IMPLEMENTATION = 4,  ///< Module implementation unit for the primary module.
};

bool readBool(const char *ptr, uint32_t &bytesRead);
uint8_t readUint8(const char *ptr, uint32_t &bytesRead);
uint32_t readUint32(const char *ptr, uint32_t &bytesRead);
uint64_t readUint64(const char *ptr, uint32_t &bytesRead);
string_view readStringView(const char *ptr, uint32_t &bytesRead);
Node *readHalfNode(const char *ptr, uint32_t &bytesRead);

void writeBool(string &buffer, const bool &value);
void writeUint8(string &buffer, const uint8_t &data);
void writeUint32(string &buffer, uint32_t data);
void writeUint64(string &buffer, uint64_t data);
void writeStringView(string &buffer, const string_view &data);
void writeNode(string &buffer, const Node *node);
void writeNodeVector(string &buffer, const vector<Node *> &array);

// Used in CppMod
inline void writeBool(std::pmr::string &buffer, const bool &value)
{
    buffer.push_back(value);
}

inline void writeUint8(std::pmr::string &buffer, const uint8_t &data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.append(ptr, ptr + sizeof(data));
}

inline void writeUint32(std::pmr::string &buffer, const uint32_t data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.append(ptr, ptr + sizeof(data));
}

inline void writeUint64(std::pmr::string &buffer, const uint64_t data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.append(ptr, ptr + sizeof(data));
}

inline void writeStringView(std::pmr::string &buffer, const string_view &data)
{
    writeUint32(buffer, data.size());
    buffer.append(data.begin(), data.end());
}

#endif // HMAKE_BASICTARGETS_HPP