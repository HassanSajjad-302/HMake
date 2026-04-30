/// \file
/// Defines `BTarget` and `RealBTarget`, the core scheduling units of HMake.

#ifndef HMAKE_BASICTARGETS_HPP
#define HMAKE_BASICTARGETS_HPP

#include "PointerPacking.h"
#include "RunCommand.hpp"
#include "gtl/include/gtl/phmap.hpp"
#include <array>
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
enum class BTargetDepKind : uint8_t
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

    /// This is not a BTargetDepKind. Used only with RealBTarget::dependencies. Some BTarget need to gather all the
    /// transitive dependencies. They can instead use the very RealBTarget::dependencies but with the following
    /// BTargetDepType.
    INDIRECT = 4,
};

class RealBTarget;

// Forward declaration — substitute your actual abstract class type enum
enum class BTargetType : unsigned short
{
    DEFAULT = 0,
    UNKNOWN = 1,
    CPPMOD = 2,
    LINK_OR_ARCHIVE_TARGET = 3,
    CPP_TARGET = 4,
};

/// RealBTargetWithType
struct RBTWithType
{
    PointerIntPair<RealBTarget *, 3, BTargetDepKind> ptrAndDepKind;
    BTargetType type{};

    RBTWithType(RealBTarget *ptr, BTargetDepKind depType, BTargetType abstractType);

    RealBTarget *getPointer() const
    {
        return ptrAndDepKind.getPointer();
    }
    BTargetDepKind getDepType() const
    {
        return ptrAndDepKind.getInt();
    }
    BTargetType getAbstractType() const
    {
        return type;
    }
};

struct RBTDepTypeHash
{
    using is_transparent = void;

    size_t operator()(const RBTWithType &e) const
    {
        return std::hash<RealBTarget *>{}(e.getPointer());
    }

    size_t operator()(RealBTarget *ptr) const
    {
        return std::hash<RealBTarget *>{}(ptr);
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

/// This is used by different BTarget to synchronize printing with build-cache updating.
enum class UpdateStatus : char
{
    ALREADY_UPDATED = 0,
    NEEDS_UPDATE = 1,
    UPDATED_WITHOUT_BUILDING = 2,
    UPDATED = 3,
};

/// Every BTarget has 2 of these so distinct dependency order can be specified for the 2 rounds.
class RealBTarget
{
    /// Contains RealBTarget* that form the cycle if there is any.
    inline static vector<RealBTarget *> cycle;

    /// Set by `sortGraph()` when the dependency graph is cyclic.
    inline static bool cycleExists = false;

  public:
    /// This is not used by build-algorithm. This is used by CppMod to check whether a header-unit is built or not. This
    /// is also used by TargetCache::writeBuildCache function to determine whether cache could be updated or not. LOAT,
    /// CppSrc and CppMod in BTarget::completeRoundOne, assign this after exitStatus to ensure happens-before
    /// relationship in the signal-handler / console-handler function. Also, this is assigned before printing to avoid a
    /// situation where a target has printed the update but its cache is not yet updated in-case of build interruption.
    alignas(64) UpdateStatus updateStatus = UpdateStatus::ALREADY_UPDATED;

  private:
    /// Mutable working counter used by `sortGraph()`.
    uint32_t dependentsCount = 0;

    /// DFS helper to report one concrete cycle path.
    static bool findCycleDFS(RealBTarget *node, gtl::flat_hash_set<RealBTarget *> &visited,
                             gtl::flat_hash_set<RealBTarget *> &recursionStack, vector<RealBTarget *> &currentPath,
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
    flat_hash_map<RealBTarget *, BTargetDepKind> dependents;
    /// Forward edges: "what I depend on".
    flat_hash_map<RealBTarget *, BTargetDepKind> dependencies;

    /// Owning high-level target.
    BTarget *bTarget = nullptr;

    /// Once sorted the index of this RealBTarget in the topological sorted array. Used in sorting to provide static
    /// libs in order as some linkers have this requirement.
    uint32_t indexInTopologicalSort = 0;

    /// This is incremented whenever a full-dependency or wait-dependency is added.
    /// It is decremented of the full-dependents or wait-dependents when the BTarget::completeRoundOne is completed
    /// And if it is zero for any of those dependents, those are added in Builder::updateBTargets list.
    uint32_t dependenciesSize = 0;

    // TODO
    //  Following describes the time taken for the completion of this task. Currently unused.
    // unsigned long timeTaken = 0;

    /// This describes the location where this RealBTarget was inserted in the Builder::updateBTargetsList. CppMod when
    /// discovers a dependency sets the pointer on that index to nullptr and re-adds the dependency CppMod in the list
    /// to prioritize the scheduling of that CppMod.
    uint32_t insertionIndex = -1;

    /// Exit code for this RealBTarget. Failures are propagated to dependents.
    int exitStatus = EXIT_SUCCESS;

    // TODO
    // Some tools like modern linker or some common release management tasks like compression supports parallel
    // execution. But HMake can't use these tools without risking overflow of thread allocation compared to hardware
    // supported threads. Algorithm is to be developed which will call updateBTarget with the number of threads
    // alloted. It will work such that total allotment never crosses the number of hardware threads. Following two
    // variables will be used as guide for the algorithm

    // Some tasks have more potential to benefit from multiple threading than others. Or they maybe executing for longer
    // than others. Users will assign such higher priority and will be allocated more of  the thread-pool compared to
    // the other.

    // float potential = 0.5f;

    // How many threads the tool supports. -1 means any. some tools may not support more than a fixed number, so
    // updateBTarget will not be passed more than supportsThread

    // short supportsThread = -1;

    /// \param bTarget_ owning `BTarget`.
    /// \param round_ Constructor will add this into `BTarget::realBTargetsGlobal[round]`.
    RealBTarget(BTarget *bTarget_, unsigned short round_);

    /// \param bTarget_ owning `BTarget`.
    /// \param round_ Constructor will add this into `BTarget::realBTargetsGlobal[round]`.
    /// \param add if false, Constructor will not add this in `BTarget::realBTargetsGlobal[round]`.
    RealBTarget(BTarget *bTarget_, unsigned short round_, bool add);

    /// Marks `FULL` dependents as `UpdateStatus::NEEDS_UPDATE`.
    void assignNeedsUpdateToDependents();
};

static_assert(alignof(RealBTarget) >= 8,
              "RealBTarget must be at least 8-byte aligned to pack BTargetDepType into pointer bits");

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
    alignas(64) inline static uint32_t total = 0;

    /// One per round: index `0` (round 0), index `1` (round 1).
    array<RealBTarget, 2> realBTargets;

    string name;
    size_t id = 0; // unique for every BTarget

    // TODO
    // Following describes total time taken across all rounds. i.e. sum of all RealBTarget::timeTaken.
    // float totalTimeTaken = 0.0f;

    /// Whether this target is selected for build in the current invocation.
    /// Computed by `setSelectiveBuild()`.
    bool selectiveBuild = false;

    /// If true, this target is selected only when explicitly requested on CLI.
    bool buildExplicit = false;

    BTarget();
    /// \param makeDirectory if true HMake will make the directory of the \p name_ in configureDir.
    BTarget(string name_, bool buildExplicit_, bool makeDirectory);
    /// \p add0 and \p add1 specify the value for RealBTarget constructors for the 2 rounds.
    BTarget(bool add0, bool add1);
    /// \param makeDirectory if true HMake will make the directory of the \p name_ in configureDir.
    BTarget(string name_, bool buildExplicit_, bool makeDirectory, bool add0, bool add1);

    virtual ~BTarget();

    /// Might set the BTarget::selectiveBuild variable based on BTarget::name, hbuild execution directory and passed
    /// arguments. Called in round1 before `completeRoundOne` call.
    void setSelectiveBuild();

    /// Returns true when `hbuild` runs in this target directory or one of its children.
    bool isHBuildInSameOrChildDirectory() const;

    /// string is used in logs and cycle diagnostics.
    virtual string getPrintName() const;

    /// Runtime classifier for derived target kinds.
    virtual BTargetType getBTargetType() const;

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

    /// Adds dependency edge for a given round and dependency type.
    template <unsigned short round, BTargetDepKind depType = BTargetDepKind::FULL> void addDep(BTarget &dep);

    /// This function is called in standAlone mode, so the BTarget could generate stand-alone commands that could be run
    /// stand-alone without the need for the build-system.
    virtual void generateStandAloneCommand();

    /// This function is called by dependents of BTarget, so the scriptFile could be populated. This script could then
    /// be run alone without HMake. Used in producing the script for compiling module-file including its dependencies.
    /// This function is not part of CppMod class because CppMod class could have some dependencies that could like to
    /// add to the script
    /// \param dirPath is the directory-path where the response files and the script file is written.
    virtual void cppStandAloneCommand(flat_hash_set<string> &createdDirs, string &scriptContents,
                                      const string &scriptDir);
};
bool operator<(const BTarget &lhs, const BTarget &rhs);

template <unsigned short round, BTargetDepKind depType> void BTarget::addDep(BTarget &dep)
{
    if (realBTargets[round].dependencies.try_emplace(&dep.realBTargets[round], depType).second)
    {
        dep.realBTargets[round].dependents.try_emplace(&this->realBTargets[round], depType);
        if constexpr (depType == BTargetDepKind::FULL || depType == BTargetDepKind::WAIT)
        {
            ++realBTargets[round].dependenciesSize;
        }
    }
}

#endif // HMAKE_BASICTARGETS_HPP
