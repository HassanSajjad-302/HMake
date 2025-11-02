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
    /// \param round_
    RealBTarget(BTarget *bTarget_, unsigned short round_);
    RealBTarget(BTarget *bTarget_, unsigned short round_, bool add);
    void assignFileStatusToDependents();
    void addInTarjanNodeBTarget(unsigned short round_);
};

enum class BTargetType : unsigned short
{
    DEFAULT = 0,
    SMFILE = 1,
    LINK_OR_ARCHIVE_TARGET = 2,
    CPP_TARGET = 3,
};

inline vector<BTarget *> postBuildSpecificationArray;

class BTarget // BTarget
{
    friend RealBTarget;
    struct LaterDep
    {
        RealBTarget *b;
        RealBTarget *dep;
        BTargetDepType type;
        // dependency or dependent
        bool doBoth;
        LaterDep(RealBTarget *b_, RealBTarget *dep_, BTargetDepType type_, bool doBoth_);
    };
    class StaticInitializationTarjanNodesBTargets
    {
      public:
        StaticInitializationTarjanNodesBTargets();

        // provide some way to get at letters_
    };

    // This is currently not needed
    /*inline static array<array<BTarget *, 10>, 3> roundEndTargets;
    inline static array<atomic<uint64_t>, 3> roundEndTargetsCount{0, 0, 0};*/

  public:
    // vector because we clear this memory at the end of the round
    inline static array<std::span<RealBTarget *>, 2> realBTargetsGlobal;
    inline static array<atomic<uint32_t>, 2> realBTargetsArrayCount{0, 0};

  private:
    inline static thread_local vector<LaterDep> laterDepsLocal;
    inline static vector<vector<LaterDep> *> laterDepsCentral{};
    friend class Builder;
    friend void constructGlobals();

    inline static StaticInitializationTarjanNodesBTargets staticStuff; // constructor runs once, single instance
  public:
    inline static uint32_t total = 0;

    array<RealBTarget, 2> realBTargets;

    string name;
    size_t id = 0; // unique for every BTarget

    // TODO
    // Following describes total time taken across all rounds. i.e. sum of all RealBTarget::timeTaken.
    // float totalTimeTaken = 0.0f;
    bool selectiveBuild = false;
    bool buildExplicit = false;

    BTarget();
    BTarget(string name_, bool buildExplicit_, bool makeDirectory);
    BTarget(bool add0, bool add1);
    BTarget(string name_, bool buildExplicit_, bool makeDirectory, bool add0, bool add1);

    virtual ~BTarget();

    void setSelectiveBuild();
    bool isHBuildInSameOrChildDirectory() const;

    void receiveNotificationPostBuildSpecification();
    static void runEndOfRoundTargets();

    virtual string getPrintName() const;
    virtual BTargetType getBTargetType() const;
    virtual void updateBTarget(class Builder &builder, unsigned short round, bool &isComplete);
    virtual void endOfRound(Builder &builder, unsigned short round);

    template <unsigned short round> void addDepNow(BTarget &dep);
    template <unsigned short round> void addSelectiveDepNow(BTarget &dep);
    template <unsigned short round> void addDepLooseNow(BTarget &dep);
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
