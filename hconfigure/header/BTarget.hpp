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

enum class BTargetDepType : uint8_t
{
    /// Build-system will wait for the dependency updateBTarget call to finish before calling
    /// the dependent updateBTarget. Plus it will set the dependent selectiveBuild if the
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

enum class UpdateStatus
{
    NEEDS_UPDATE,
    ALREADY_UPDATED,
    UPDATED,
};

class RealBTarget
{
    inline static vector<RealBTarget *> cycle;
    inline static bool cycleExists = false;
    // used in sorting
    uint32_t dependentsCount = 0;

    static bool findCycleDFS(RealBTarget *node, phmap::flat_hash_set<RealBTarget *> &visited,
                             phmap::flat_hash_set<RealBTarget *> &recursionStack, vector<RealBTarget *> &currentPath,
                             string &errorString);

  public:
    inline static vector<RealBTarget *> sorted;

    // Input
    inline static std::span<RealBTarget *> graphEdges;

    // Find Strongly Connected Components
    static void sortGraph();

    // for debugging
    static void printSortedGraph();

    flat_hash_map<RealBTarget *, BTargetDepType> dependents;
    flat_hash_map<RealBTarget *, BTargetDepType> dependencies;

    BTarget *bTarget = nullptr;

    uint32_t indexInTopologicalSort = 0;
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

    UpdateStatus updateStatus = UpdateStatus::ALREADY_UPDATED;

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
