#ifndef HMAKE_BASICTARGETS_HPP
#define HMAKE_BASICTARGETS_HPP
#ifdef USE_HEADER_UNITS
import "TargetType.hpp";
import "TarjanNode.hpp";
import <array>;
import <atomic>;
import <filesystem>;
import <map>;
import <mutex>;
#else
#include "TargetType.hpp"
#include "TarjanNode.hpp"
#include <array>
#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#endif

using std::filesystem::path, std::size_t, std::map, std::mutex, std::lock_guard, std::atomic_flag, std::array,
    std::atomic;

// TBT = TarjanNodeBTarget    TCT = TarjanNodeCTarget
TarjanNode(const BTarget *) -> TarjanNode<BTarget>;
using TBT = TarjanNode<BTarget>;
inline vector<vector<TBT *>> tarjanNodesBTargets;
inline vector<mutex *> tarjanNodesBTargetsMutexes;

class StaticInitializationTarjanNodesBTargets
{
  public:
    StaticInitializationTarjanNodesBTargets();

    // provide some way to get at letters_
};

struct RealBTarget;
struct IndexInTopologicalSortComparatorRoundZero
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

inline StaticInitializationTarjanNodesBTargets staticStuff; // constructor runs once, single instance
struct RealBTarget : TBT
{
    map<BTarget *, BTargetDepType> dependents;
    map<BTarget *, BTargetDepType> dependencies;

    BTarget *bTarget = nullptr;

    unsigned int indexInTopologicalSort = 0;
    atomic<unsigned int> dependenciesSize = 0;

    // TODO
    //  Following describes the time taken for the completion of this task. Currently unused. how to determine cpu time
    //  for this task in multi-threaded scenario
    unsigned long timeTaken = 0;

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
    float potential = 0.5f;

    // How many threads the tool supports. -1 means any. some tools may not support more than a fixed number, so
    // updateBTarget will not passed more than supportsThread
    short supportsThread = -1;

    unsigned short round;

    explicit RealBTarget(BTarget *bTarget_, unsigned short round);
    template <typename... U> void addDependency(BTarget &dependency, U &...bTargets);
    template <typename... U> void addLooseDependency(BTarget &dependency, U &...bTargets);
};

namespace BTargetNamespace
{
inline std::mutex addDependencyMutex;
}

enum class BTargetType : unsigned short
{
    SMFILE = 1,
};

struct BTarget // BTarget
{
    inline static size_t total = 0;

    array<RealBTarget, 3> realBTargets;

    pstring targetSubDir;
    size_t id = 0; // unique for every BTarget

    // TODO
    // Following describes total time taken across all rounds. i.e. sum of all RealBTarget::timeTaken.
    float totalTimeTaken = 0.0f;
    bool selectiveBuild = false;
    std::atomic<bool> fileStatus = false;

    explicit BTarget();
    explicit BTarget(pstring name_, bool buildExplicit, bool makeDirectory);
    BTarget(const BTarget &) = delete;
    BTarget &operator=(const BTarget &) = delete;
    BTarget &operator=(BTarget &&) = delete;
    BTarget(BTarget &&) = delete;
    virtual ~BTarget();

    void setSelectiveBuild();
    bool getSelectiveBuildChildDir();

    virtual pstring getTarjanNodeName() const;

    virtual BTargetType getBTargetType() const;
    static void assignFileStatusToDependents(RealBTarget &realBTarget);
    virtual void updateBTarget(class Builder &builder, unsigned short round);
};
bool operator<(const BTarget &lhs, const BTarget &rhs);

inline std::mutex realbtarget_adddependency;
template <typename... U> void RealBTarget::addDependency(BTarget &dependency, U &...bTargets)
{
    {
        lock_guard lk{realbtarget_adddependency};
        // adding in both dependencies and deps is duplicating. One should be removed.
        if (dependencies.try_emplace(&dependency, BTargetDepType::FULL).second)
        {
            RealBTarget &dependencyRealBTarget = dependency.realBTargets[round];
            dependencyRealBTarget.dependents.try_emplace(bTarget, BTargetDepType::FULL);
            ++dependenciesSize;
            deps.emplace(&dependencyRealBTarget);
        }
    }

    if constexpr (sizeof...(bTargets))
    {
        addDependency(bTargets...);
    }
}

template <typename... U> void RealBTarget::addLooseDependency(BTarget &dependency, U &...bTargets)
{
    lock_guard<mutex> lk{realbtarget_adddependency};
    if (dependencies.try_emplace(&dependency, BTargetDepType::LOOSE).second)
    {
        RealBTarget &dependencyRealBTarget = dependency.realBTargets[round];
        dependencyRealBTarget.dependents.try_emplace(bTarget, BTargetDepType::LOOSE);
        deps.emplace(&dependencyRealBTarget);
    }

    if constexpr (sizeof...(bTargets))
    {
        addDependency(bTargets...);
    }
}

#endif // HMAKE_BASICTARGETS_HPP
