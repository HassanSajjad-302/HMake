#ifndef HMAKE_BASICTARGETS_HPP
#define HMAKE_BASICTARGETS_HPP
#ifdef USE_HEADER_UNITS
import "parallel-hashmap/parallel_hashmap/phmap.h";
import <vector>;
import <array>;
import <atomic>;
import <string>;
import <mutex>;
#else
#include "parallel-hashmap/parallel_hashmap/phmap.h"
#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#endif

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

enum class BTargetDepType : bool
{
    FULL,

    // Following specifies a dependency only for ordering. That dependency won't be considered for selectiveBuild, and,
    // won't be updated when the dependencies are updated, and, will still be updated even if dependency exitStatus ==
    // EXIT_FAILURE. Only usable in round 0.
    LOOSE,
};

class RealBTarget
{
    // For TarjanNode topological sorting
    // Following 4 are reset in findSCCS();
    inline static int index = 0;
    inline static vector<RealBTarget *> nodesStack;

    // Output
    inline static vector<BTarget *> cycle;
    inline static bool cycleExists;

  public:
    inline static vector<BTarget *> topologicalSort;

    // Input
    inline static vector<RealBTarget *> *tarjanNodes;

    // Find Strongly Connected Components
    static void findSCCS(unsigned short round);
    static void checkForCycle();
    static void clearTarjanNodes();

    flat_hash_map<BTarget *, BTargetDepType> dependents;
    flat_hash_map<BTarget *, BTargetDepType> dependencies;

    BTarget *bTarget = nullptr;

    unsigned int indexInTopologicalSort = 0;
    unsigned int dependenciesSize = 0;

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

  private:
    void strongConnect(unsigned short round);
    int nodeIndex = 0;
    int lowLink = 0;
    bool initialized = false;
    bool onStack = false;

  public:
    // Set it to false, and build-system will not decrement dependenciesSize of the dependents RealBTargets
    bool isUpdated = true;

    RealBTarget(BTarget *bTarget_, unsigned short round_);
    RealBTarget(BTarget *bTarget_, unsigned short round_, bool add);
    void addInTarjanNodeBTarget(unsigned short round_);
};

enum class BTargetType : unsigned short
{
    DEFAULT = 0,
    SMFILE = 1,
    LINK_OR_ARCHIVE_TARGET = 2,
};

inline vector<BTarget *> postBuildSpecificationArray;

class BTarget // BTarget
{
    friend RealBTarget;
    struct TwoBTargets
    {
        BTarget *b;
        BTarget *dep;
    };
    class StaticInitializationTarjanNodesBTargets
    {
      public:
        StaticInitializationTarjanNodesBTargets();

        // provide some way to get at letters_
    };

    // This not needed currently
    /*inline static array<array<BTarget *, 10>, 3> roundEndTargets;
    inline static array<atomic<uint64_t>, 3> roundEndTargetsCount{0, 0, 0};*/

  public:
    // vector because we clear this memory at the end of the round
    inline static array<vector<RealBTarget *>, 3> tarjanNodesBTargets;
    inline static array<atomic<uint32_t>, 3> tarjanNodesCount{0, 0, 0};

  private:
    inline static array<vector<TwoBTargets>, 2> twoBTargetsVector;
    inline static array<atomic<uint32_t>, 2> twoBTargetsVectorSize{0, 0};

    inline static StaticInitializationTarjanNodesBTargets staticStuff; // constructor runs once, single instance
  public:
    inline static size_t total = 0;

    array<RealBTarget, 3> realBTargets;

    string name;
    size_t id = 0; // unique for every BTarget

    // TODO
    // Following describes total time taken across all rounds. i.e. sum of all RealBTarget::timeTaken.
    // float totalTimeTaken = 0.0f;
    bool selectiveBuild = false;
    bool fileStatus = false;
    bool buildExplicit = false;

    BTarget();
    BTarget(string name_, bool buildExplicit_, bool makeDirectory);
    BTarget(bool add0, bool add1, bool add2);
    BTarget(string name_, bool buildExplicit_, bool makeDirectory, bool add0, bool add1, bool add2);

    virtual ~BTarget();

    void setSelectiveBuild();
    bool isHBuildInSameOrChildDirectory() const;

    void assignFileStatusToDependents(unsigned short round);
    void receiveNotificationPostBuildSpecification();
    static void runEndOfRoundTargets(class Builder &builder, uint16_t round);

    virtual string getTarjanNodeName() const;
    virtual BTargetType getBTargetType() const;
    virtual void updateBTarget(class Builder &builder, unsigned short round);
    virtual void endOfRound(Builder &builder, unsigned short round);
    virtual void copyJson();
    virtual void buildSpecificationCompleted();

    template <unsigned short round> void addEndOfRoundBTarget();

    template <unsigned short round, typename... U> void addDependency(BTarget &dependency, U &...bTargets);
    template <unsigned short round, typename... U> void addLooseDependency(BTarget &dependency, U &...bTargets);

    template <unsigned short round, typename... U> void addDependencyDelayed(BTarget &dependency, U &...bTargets);
    template <unsigned short round> void addDependencyNoMutex(BTarget &dependency);
};
bool operator<(const BTarget &lhs, const BTarget &rhs);

inline std::mutex dependencyMutex[3];

template <unsigned short round> void BTarget::addEndOfRoundBTarget()
{
    /*const uint64_t i = roundEndTargetsCount[round].fetch_add(1);
    roundEndTargets[round][i] = this;*/
}

template <unsigned short round, typename... U> void BTarget::addDependency(BTarget &dependency, U &...bTargets)
{
    {
        lock_guard lk{dependencyMutex[round]};
        if (realBTargets[round].dependencies.try_emplace(&dependency, BTargetDepType::FULL).second)
        {
            RealBTarget &dependencyRealBTarget = dependency.realBTargets[round];
            dependencyRealBTarget.dependents.try_emplace(this, BTargetDepType::FULL);
            ++atomic_ref(realBTargets[round].dependenciesSize);
        }
    }

    if constexpr (sizeof...(bTargets))
    {
        addDependency<round>(bTargets...);
    }
}

template <unsigned short round, typename... U> void BTarget::addLooseDependency(BTarget &dependency, U &...bTargets)
{
    lock_guard lk{dependencyMutex[round]};
    if (realBTargets[round].dependencies.try_emplace(&dependency, BTargetDepType::LOOSE).second)
    {
        RealBTarget &dependencyRealBTarget = dependency.realBTargets[round];
        dependencyRealBTarget.dependents.try_emplace(this, BTargetDepType::LOOSE);
    }

    if constexpr (sizeof...(bTargets))
    {
        addLooseDependency<round>(bTargets...);
    }
}

template <unsigned short round, typename... U> void BTarget::addDependencyDelayed(BTarget &dependency, U &...bTargets)
{
    twoBTargetsVector[round][twoBTargetsVectorSize[round].fetch_add(1)] = TwoBTargets{.b = this, .dep = &dependency};

    if constexpr (sizeof...(bTargets))
    {
        addDependencyPost<round>(bTargets...);
    }
}

template <unsigned short round> void BTarget::addDependencyNoMutex(BTarget &dependency)
{
    // adding in both dependencies and deps is duplicating. One should be removed.
    if (realBTargets[round].dependencies.try_emplace(&dependency, BTargetDepType::FULL).second)
    {
        RealBTarget &dependencyRealBTarget = dependency.realBTargets[round];
        dependencyRealBTarget.dependents.try_emplace(this, BTargetDepType::FULL);
        ++atomic_ref(realBTargets[round].dependenciesSize);
    }
}

#endif // HMAKE_BASICTARGETS_HPP
