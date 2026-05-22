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
    std::string_view;

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

inline std::pmr::monotonic_buffer_resource pool(
    2 * 1024 * 1024,
    std::pmr::null_memory_resource() // throws std::bad_alloc instead of falling back to heap
);

using DepSet =
    flat_hash_set<RBTWithType, RBTDepTypeHash, RBTDepTypeEqual, std::pmr::polymorphic_allocator<RBTWithType>>;

enum class UpdateStatus : char
{
    UNCHECKED = 0,
    UPDATED_NEEDED = 1,
    UPDATE_NOT_NEEDED = 2,
};

/// Every BTarget has 2 of these so distinct dependency order can be specified for the 2 rounds.
class alignas(128) RealBTarget
{
    /// Contains RealBTarget* that form the cycle if there is any.
    inline static vector<RealBTarget *> cycle;

    /// Set by `sortGraph()` when the dependency graph is cyclic.
    inline static bool cycleExists = false;

    /// DFS helper to report one concrete cycle path.
    static bool findCycleDFS(RealBTarget *node, vector<RealBTarget *> &currentPath, string &errorString);

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

    uint64_t cumulativeHash = 0;
    uint64_t launchTime = -1;

    /// Once sorted the index of this RealBTarget in the topological sorted array. Used in sorting to provide static
    /// libs in order as some linkers have this requirement.
    uint32_t indexInTopologicalSort : 29 = 0;

    /// This is set to UpdateStatus::UPDATED in Builder::decrementFromDependents. This function also sets the value for
    /// the dependents to UpdateStatus::NEEDS_UPDATE if ours was UpdateStatus::NEEDS_UPDATE.
    UpdateStatus updateStatus : 3 = UpdateStatus::UNCHECKED;

    /// This is incremented whenever a full-dependency or wait-dependency is added.
    /// It is decremented of the full-dependents or wait-dependents when the BTarget::completeRoundOne is completed
    /// And if it is zero for any of those dependents, those are added in Builder::updateBTargets list.
    uint32_t dependenciesSize : 30 = 0;

    bool isCompleted : 1 = false;

    /// Which element of BTarget::realBTargets[2] this instance occupies (0 or 1).
    /// Used by getBTarget() to recover the enclosing BTarget via pointer arithmetic.
    bool round : 1 = 0;

    // TODO
    //  Following describes the time taken for the completion of this task. Currently unused.
    // unsigned long timeTaken = 0;

    /// This describes the location where this RealBTarget was inserted in the Builder::updateBTargetsList. CppMod when
    /// discovers a dependency sets the pointer on that index to nullptr and re-adds the dependency CppMod in the list
    /// to prioritize the scheduling of that CppMod.
    uint32_t insertionIndex = -1;

    /// Exit code for this RealBTarget. Failures are propagated to dependents.
    bool exitStatus = EXIT_SUCCESS;

    /// \param round_ which slot in BTarget::realBTargets this occupies (0 or 1).
    /// Constructor will add this into `BTarget::realBTargetsGlobal[round]`.
    RealBTarget(unsigned short round_);

    /// \param round which slot in BTarget::realBTargets this occupies (0 or 1).
    /// \param add if false, Constructor will not add this in `BTarget::realBTargetsGlobal[round]`.
    RealBTarget(unsigned short round_, bool add);
    bool checkDepsChanged() const;

    /// Adds dependency edge for a given round and dependency type.
    template <BTargetType type, RelationType depType = RelationType::FULL> void addDep(RealBTarget *dep);

    /// Recovers the enclosing BTarget via compile-time offset arithmetic.
    /// Zero-cost: no stored pointer, no virtual dispatch.
    BTarget *getBTarget() const;
};

static_assert(alignof(RealBTarget) >= 128,
              "RealBTarget must be 128-byte aligned to pack 7 bits (3 RelationType + 4 BTargetType) into pointer");

/// Base class for all build graph tasks.
///
/// Derived classes override one or more execution hooks:
/// - `completeRoundOne()` for synchronous round-1 work
/// - `isEventRegistered()` / `isEventCompleted()` for event-driven round-0 work
///
/// Ordering constraints are declared with `addDep`.
class BTarget // BTarget
{
    friend RealBTarget;

  public:
    /// Per-round global storage of registered nodes.
    /// Populated by `RealBTarget` constructors where `add == true`.
    inline static array<std::span<RealBTarget *>, 2> realBTargetsGlobal;

    /// Current insertion count for each round in `realBTargetsGlobal`.
    inline static array<uint32_t, 2> realBTargetsArrayCount{};

    // todo
    // Maybe 3 of the RealBTarget[2] and the following could be pointers instead.

    /// Process/event helper used by round-0 async workflows.
    RunCommand run;

  private:
    friend class Builder;
    friend void constructGlobals();

  public:
    inline static uint32_t total = 0;

    string name;
    size_t id = 0; // unique for every BTarget

    /// Address of this element in fileTargetCaches vector
    uint32_t cacheIndex = -1;
    bool buildCacheUpdated = false;
    bool buildFooterUpdated = false;

    /// Name in the config-cache
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

    bool newlyAdded = false;

    array<RealBTarget, 2> realBTargets;

    void initializeBTarget(bool makeDirectory);

    /// One per round: index `0` (round 0), index `1` (round 1).
    BTarget(string name_, bool launchesProcess_, BTargetType type_);
    /// \param makeDirectory if true HMake will make the directory of the \p name_ in configureDir.
    BTarget(string name_, bool launchesProcess_, BTargetType type_, bool buildExplicit_, bool makeDirectory);
    /// \param makeDirectory if true HMake will make the directory of the \p name_ in configureDir.
    BTarget(string name_, bool launchesProcess_, BTargetType type_, bool buildExplicit_, bool makeDirectory, bool add0,
            bool add1);

    /// One per round: index `0` (round 0), index `1` (round 1).
    BTarget(string name_, uint64_t cacheName_, bool launchesProcess_, BTargetType type_);
    /// \param makeDirectory if true HMake will make the directory of the \p name_ in configureDir.
    BTarget(string name_, uint64_t cacheName_, bool launchesProcess_, BTargetType type_, bool buildExplicit_,
            bool makeDirectory);
    /// \param makeDirectory if true HMake will make the directory of the \p name_ in configureDir.
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

    virtual void setFileStatus();

    /// This function is called in standAlone mode, so the BTarget could generate stand-alone commands that could be
    /// run stand-alone without the need for the build-system.
    virtual void generateStandAloneCommand();

    /// This function is called by dependents of BTarget, so the scriptFile could be populated. This script could then
    /// be run alone without HMake. Used in producing the script for compiling module-file including its dependencies.
    /// This function is not part of CppMod class because CppMod class could have some dependencies that could like to
    /// add to the script
    /// \param dirPath is the directory-path where the response files and the script file is written.
    virtual void cppStandAloneCommand(flat_hash_set<string> &createdDirs, string &scriptContents,
                                      const string &scriptDir);

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

    // Accumulates verbose parse output; flushed to stdout only when verifyError() fires.
    inline static string verifyOutput;

    [[noreturn]] static void verifyError(const string &msg)
    {
        printMessage(verifyOutput);
        printErrorMessage(msg); // calls errorExit()
    }
};
bool operator<(const BTarget &lhs, const BTarget &rhs);

template <BTargetType type, RelationType depType> void RealBTarget::addDep(RealBTarget *dep)
{
    if (dependencies.emplace(dep, depType, type).second)
    {
        dep->dependents.emplace(this, depType, type);
        if constexpr (depType == RelationType::FULL || depType == RelationType::WAIT)
        {
            ++dependenciesSize;
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

/// HMake has 2 different cache that are config-cache and build-cache. config-cache is only read but not written at
/// build-time. By separating config-cache and build-cache, we keep the build-cache smaller and keep the builds faster.
///
/// TargetCache works with FileTargetCache to manage the config-cache and build-cache reading and writing. Any class
/// that wants to store cache in config-cache or build-cache should inherit from this class. Its constructor take a name
/// that must be unique for 2 inherited objects. Otherwise, configure step will fail.
///
/// In config-cache, we store name, then the size of the data and then the data itself.
/// However, in build-cache, we store only the size and the data but not the name. The order is same in both
/// config-cache and build-cache.
///
/// initializeBuildCache() calls first readConfigCache() and then readBuildCache().
///
/// readConfigCache() populates fileTargetCaches and nameToIndexMap containers. Then in buildSpecification() and
/// later, whenever an object of TargetCache is instantiated, in the TargetCache constructor, we use nameToIndexMap
/// container to retrieve the index of FileTargetCache element in fileTargetCaches container. FileTargetCache contains
/// the both configCache and buildCache data that were populated in readConfigCache() and readBuildCache() respectively.

/// Every Target-Cache has representation on both config-cache and build-cache. It could be empty. The order is
/// persistent across builds.
class FileTargetCache
{
  public:
    /// Back-pointer to the bTarget
    BTarget *bTarget = nullptr;
    uint64_t name;
    string_view configCache;
    string_view depsCache;

  private:
    string_view buildCache;

  public:
    string_view getBuildCache() const
    {
        return string_view{buildCache.data(), buildCache.size() - 16};
    }
    string_view getFullBuildCache() const
    {
        return buildCache;
    }
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

inline vector<FileTargetCache> fileTargetCaches;
inline flat_hash_map<uint64_t, uint32_t> nameToIndexMap;

enum class CppModType : uint8_t
{
    CPP_SRC = 0,
    PRIMARY_EXPORT = 1,
    PARTITION_EXPORT = 2,
    HEADER_UNIT = 3,
    PRIMARY_IMPLEMENTATION = 4,
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