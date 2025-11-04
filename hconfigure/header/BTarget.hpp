/// \file
/// Define BTarget and RealBTarget, core classes of HMake

#ifndef HMAKE_BASICTARGETS_HPP
#define HMAKE_BASICTARGETS_HPP

#include "parallel-hashmap/parallel_hashmap/phmap.h"
#include <array>
#include <atomic>
#include <mutex>
#include <span>
#include <string>
#include <vector>

using std::size_t, std::vector, phmap::flat_hash_map, std::mutex, std::lock_guard, std::atomic_flag, std::array,
    std::atomic, std::atomic_ref, std::string;

class BTarget;

// TODO
// Maybe remove this and use vector with in-place sorting.
struct IndexInTopologicalSortComparatorRoundZero
{
    bool operator()(const BTarget *lhs, const BTarget *rhs) const;
};

struct IndexInTopologicalSortComparatorRoundTwo
{
    bool operator()(const BTarget *lhs, const BTarget *rhs) const;
};

/// Used in RealBTarget to specify the type of dependency of other RealBTargets.
enum class BTargetDepType : uint8_t
{
    /// Build-system will wait for the dependency BTarget::updateBTarget call to finish before calling
    /// the dependent BTarget::updateBTarget. Plus it will set the dependent selectiveBuild if the
    /// dependency selectiveBuild is true
    FULL = 0,

    /// Build-system will wait but not set the selectiveBuild of dependent based on dependency.
    /// Unused currently
    WAIT = 1,

    /// Build-system will not wait but the selectiveBuild will be set.
    /// Used in specifying CppSourceTarget dep with the other CppSourceTarget as we want
    /// the dependency CppSourceTarget's huDeps and imodDeps to be built.
    SELECTIVE = 2,

    /// Only for sorting.
    /// Used to specify static-lib dependency with other static-lib.
    LOOSE = 3,
};

/// Used in RealBTarget to convey the status about BTarget::updateBTarget() function execution completion
enum class UpdateStatus
{
    /// RealBTarget::updateStatus is defaulted to this
    ALREADY_UPDATED,
    /// RealBTarget::updateStatus is set to this once the BTarget::updateBTarget call is completed.
    /// SMFile tests RealBTarget::updateStatus against this to confirm whether the dependency module or header-unit is
    /// updated or not.
    UPDATED,
    /// This is an additional value that is used by SourceNode and SMFile to store whether the file needs to be
    /// recompiled
    NEEDS_UPDATE,
};

/// Every BTarget has 2 of these so distinct dependency order can be specified for the 2 rounds.
class RealBTarget
{
    /// Contains RealBTarget* that form the cycle if there is any.
    inline static vector<RealBTarget *> cycle;

    /// it is set by sortGraph function if there is a cycle in the graph
    inline static bool cycleExists = false;

    // used in sorting
    uint32_t dependentsCount = 0;

    /// if there is a cycle, find out the exact RealBTarget involved
    static bool findCycleDFS(RealBTarget *node, phmap::flat_hash_set<RealBTarget *> &visited,
                             phmap::flat_hash_set<RealBTarget *> &recursionStack, vector<RealBTarget *> &currentPath,
                             string &errorString);

  public:
    /// sorted RealBTargets
    inline static vector<RealBTarget *> sorted;

    /// Input to the sortGraph function.
    inline static std::span<RealBTarget *> graphEdges;

    static void sortGraph();

    // for debugging
    static void printSortedGraph();

    flat_hash_map<RealBTarget *, BTargetDepType> dependents;
    flat_hash_map<RealBTarget *, BTargetDepType> dependencies;

    /// reverse pointer to the BTarget
    BTarget *bTarget = nullptr;

    /// Once sorted the index of this RealBTarget in the topological sorted array.
    /// used in sorting and providing static libs in order as some linkers have this requirement.
    uint32_t indexInTopologicalSort = 0;

    /// This is incremented whenever a full-dependency or wait-dependency is added
    /// It is decremented of the full-dependents or wait-dependents when the BTarget::updateBTarget is completed
    /// And if it is zero for any of those dependents, those are added in Builder::updateBTargets list.
    uint32_t dependenciesSize = 0;

    // TODO
    //  Following describes the time taken for the completion of this task. Currently unused. how to determine cpu time
    //  for this task in multi-threaded scenario
    // unsigned long timeTaken = 0;

    // Plays two roles. Depicts the exitStatus of itself and of its dependencies
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
    // updateBTarget will not passed more than supportsThread

    // short supportsThread = -1;

    /// Initialized to ALREADY_UPDATED and then set to UpdateStatus::UPDATED once the BTarget::updateBTarget call is
    /// completed. This is used by SMFile to learn whether a header-units is built or not.
    UpdateStatus updateStatus = UpdateStatus::ALREADY_UPDATED;

    /// \param bTarget_ the back-pointer to BTarget that owns this
    /// \param round_ Constructor will add to BTarget::realBTargetsGlobal with the round index.
    RealBTarget(BTarget *bTarget_, unsigned short round_);

    /// \param bTarget_ the back-pointer to BTarget that owns this
    /// \param round_ Constructor will add to BTarget::realBTargetsGlobal[round]
    /// \param add whether to add for a round. Should be false if BTarget::updateBTarget is not going to do any work in
    /// that round. SourceNode and SMFile initialize BTarget::realBTargets[1] with this parameter as false as they have
    /// work only in round 0. Specifying a RealBTarget with add as false as dependency or dependent of other RealBTarget
    /// is undefined behavior.
    RealBTarget(BTarget *bTarget_, unsigned short round_, bool add);

    /// Assigns full-dependents RealBTarget::updateStatus with UpdateStatus::NEEDS_UPDATE.
    /// This is used by SourceNode and SMFile so that the LOAT does not have to make an extra check.
    void assignNeedsUpdateToDependents();
};

enum class BTargetType : unsigned short
{
    DEFAULT = 0,
    SMFILE = 1,
    LINK_OR_ARCHIVE_TARGET = 2,
    CPP_TARGET = 3,
};

/// The building-block of HMake build-system
///
/// Any class that wants to perform work in order needs to inherit from this and override BTarget::updateBTarget
/// function. Then specify the dependencies between 2 BTargets using this class functions. Now the build-system will
/// automatically call the BTarget::updateStatus of the both BTargets in-order.
class BTarget // BTarget
{
    friend RealBTarget;

    /// This class helps in concurrent dependency specification between 2 BTargets. This removes the need for using
    /// mutex based synchronization.
    ///
    /// Specifying dependencies is 2-step process of adding a value in RealBTarget::dependencies and
    /// RealBTarget::dependents. Doing this in multiple threads in BTarget::updateBTarget is not thread-safe. While
    /// doing it under mutex is too slow.
    /// This class helps mitigate this with the following invariant.
    /// A BTarget can only emplace in RealBTarget::dependencies of itself in BTarget::updateBTarget. This first step
    /// is thread-safe. However, it can not do the second step of emplace in the dependent RealBTarget's
    /// RealBTarget::dependents. Instead, it adds a new LaterDep in the thread_local BTarget::laterDepsLocal.
    /// Builder::execute will then call BTarget::postRoundCompletion which will go over this array and complete the
    /// dependency-specification for the round 0.
    struct LaterDep
    {
        RealBTarget *b;
        RealBTarget *dep;
        BTargetDepType type;
        bool doBoth;

        /// \param doBoth_ This will be true if the BTarget::updateBTarget wants to specify dependency between 2 other
        /// BTargets. In that case both steps of dependency-specification will be performed instead of just the second
        /// step, emplace in dep->dependents.
        LaterDep(RealBTarget *b_, RealBTarget *dep_, BTargetDepType type_, bool doBoth_);
    };

  public:
    /// All the RealBTargets of a round except those whose add is false. BTarget::updateBTarget will be called for these
    /// in-order after sorting. This is populated by the RealBTarget constructor.
    inline static array<std::span<RealBTarget *>, 2> realBTargetsGlobal;

    /// count of BTarget::realBTargetsGlobal and the index where the pointer to the self will be added by the
    /// RealBTarget constructor.
    inline static array<atomic<uint32_t>, 2> realBTargetsArrayCount{0, 0};

  private:
    /// An array of dependency relationships that are specified in single-thread. Generally half are specified in
    /// multi-thread while the other half is specified in single-thread after round 1. Populated by addDep* functions.
    inline static thread_local vector<LaterDep> laterDepsLocal;

    /// Pointer to all the thread_local BTarget::laterDepsLocal entries. Populated in Builder::Builder.
    inline static vector<vector<LaterDep> *> laterDepsCentral{};
    friend class Builder;
    friend void constructGlobals();

  public:
    inline static uint32_t total = 0;

    /// One RealBTarget for every round.
    array<RealBTarget, 2> realBTargets;

    string name;
    size_t id = 0; // unique for every BTarget

    // TODO
    // Following describes total time taken across all rounds. i.e. sum of all RealBTarget::timeTaken.
    // float totalTimeTaken = 0.0f;

    /// selectiveBuild is set for target if hbuild is executed in the target directory or the target name is provided to
    /// the build.exe or hbuild. It is set in BTarget::setSelectiveBuild which is call before BTarget::updateBTarget of
    /// round1.
    bool selectiveBuild = false;

    /// if true selectiveBuild is only set if the target name is specified on cmd arguments.
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
    /// arguments.
    void setSelectiveBuild();

    /// returns true if hbuild is executed in same or child directory based on BTarget::name.
    bool isHBuildInSameOrChildDirectory() const;

    /// Called by Builder::execute post round1.
    static void postRoundOneCompletion();

    /// Used by findCycleDFS to report RealBTarget in cycle
    virtual string getPrintName() const;

    /// Can be overridden to differentiate between different BTarget class objects
    virtual BTargetType getBTargetType() const;

    /// This is called by Builder in-order. Should be overridden to perform any work
    virtual void updateBTarget(class Builder &builder, unsigned short round, bool &isComplete);

    /// Does both steps. Should be called only in single-thread
    template <unsigned short round> void addDepNow(BTarget &dep);
    template <unsigned short round> void addSelectiveDepNow(BTarget &dep);
    template <unsigned short round> void addDepLooseNow(BTarget &dep);

    /// Does one step. Adds the second step in laterDeps to be called later in single-thread
    void addDepHalfNowHalfLater(BTarget &dep);
    void addDepLooseHalfNowHalfLater(BTarget &dep);
    void addDepLater(BTarget &dep);
    void addDepLooseLater(BTarget &dep);
};
bool operator<(const BTarget &lhs, const BTarget &rhs);

template <unsigned short round> void BTarget::addDepNow(BTarget &dep)
{
    if (realBTargets[round].dependencies.try_emplace(&dep.realBTargets[round], BTargetDepType::FULL).second)
    {
        RealBTarget &depRealBTarget = dep.realBTargets[round];
        depRealBTarget.dependents.try_emplace(&this->realBTargets[round], BTargetDepType::FULL);
        ++realBTargets[round].dependenciesSize;
    }
}

template <unsigned short round> void BTarget::addSelectiveDepNow(BTarget &dep)
{
    if (realBTargets[round].dependencies.try_emplace(&dep.realBTargets[round], BTargetDepType::SELECTIVE).second)
    {
        RealBTarget &depRealBTarget = dep.realBTargets[round];
        depRealBTarget.dependents.try_emplace(&this->realBTargets[round], BTargetDepType::SELECTIVE);
    }
}

template <unsigned short round> void BTarget::addDepLooseNow(BTarget &dep)
{
    if (realBTargets[round].dependencies.try_emplace(&dep.realBTargets[round], BTargetDepType::LOOSE).second)
    {
        RealBTarget &depRealBTarget = dep.realBTargets[round];
        depRealBTarget.dependents.try_emplace(&this->realBTargets[round], BTargetDepType::LOOSE);
    }
}

#endif // HMAKE_BASICTARGETS_HPP
