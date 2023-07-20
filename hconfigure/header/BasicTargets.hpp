#ifndef HMAKE_BASICTARGETS_HPP
#define HMAKE_BASICTARGETS_HPP
#ifdef USE_HEADER_UNITS
import "C_API.hpp";
import "TargetType.hpp";
import "TarjanNode.hpp";
import <atomic>;
import <filesystem>;
import <map>;
import <mutex>;
#else
#include "C_API.hpp"
#include "TargetType.hpp"
#include "TarjanNode.hpp"
#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#endif

using std::filesystem::path, std::size_t, std::map, std::mutex, std::lock_guard, std::atomic_flag;

// TBT = TarjanNodeBTarget    TCT = TarjanNodeCTarget
TarjanNode(const struct BTarget *) -> TarjanNode<BTarget>;
using TBT = TarjanNode<BTarget>;
inline vector<set<TBT>> tarjanNodesBTargets;
inline vector<mutex *> tarjanNodesBTargetsMutexes;

TarjanNode(const class CTarget *) -> TarjanNode<CTarget>;
using TCT = TarjanNode<CTarget>;
inline set<TCT> tarjanNodesCTargets;

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
    // EXIT_FAILURE. Only useable in round 0.
    LOOSE,
};

inline StaticInitializationTarjanNodesBTargets staticStuff; // constructor runs once, single instance
struct RealBTarget
{
    map<BTarget *, BTargetDepType> dependents;
    map<BTarget *, BTargetDepType> dependencies;

    // This points to the tarjanNodeBTargets set element
    TBT *bTarjanNode = nullptr;
    BTarget *bTarget = nullptr;

    unsigned int indexInTopologicalSort = 0;
    unsigned int dependenciesSize = 0;

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
    void setBTarjanNode();
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

    vector<RealBTarget> realBTargets;

    size_t id = 0; // unique for every BTarget

    // TODO
    // Following describes total time taken across all rounds. i.e. sum of all RealBTarget::timeTaken.
    float totalTimeTaken = 0.0f;
    bool selectiveBuild = false;
    std::atomic<bool> fileStatus = false;

    explicit BTarget();
    virtual ~BTarget();

    virtual pstring getTarjanNodeName() const;

    virtual BTargetType getBTargetType() const;
    void assignFileStatusToDependents(RealBTarget &realBTarget) const;
    virtual void preSort(class Builder &builder, unsigned short round);
    virtual void updateBTarget(Builder &builder, unsigned short round);
};
bool operator<(const BTarget &lhs, const BTarget &rhs);

inline std::mutex realbtarget_adddependency;
template <typename... U> void RealBTarget::addDependency(BTarget &dependency, U &...bTargets)
{
    {
        lock_guard<mutex> lk{realbtarget_adddependency};
        if (dependencies.try_emplace(&dependency, BTargetDepType::FULL).second)
        {
            RealBTarget &dependencyRealBTarget = dependency.realBTargets[round];
            dependencyRealBTarget.dependents.try_emplace(bTarget, BTargetDepType::FULL);
            ++dependenciesSize;
            setBTarjanNode();
            dependencyRealBTarget.setBTarjanNode();
            bTarjanNode->deps.emplace(dependencyRealBTarget.bTarjanNode);
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
        setBTarjanNode();
        dependencyRealBTarget.setBTarjanNode();
        bTarjanNode->deps.emplace(dependencyRealBTarget.bTarjanNode);
    }

    if constexpr (sizeof...(bTargets))
    {
        addDependency(bTargets...);
    }
}

struct CTargetPointerComparator
{
    bool operator()(const CTarget *lhs, const CTarget *rhs) const;
};

class CTarget // Configure Target
{
    inline static set<std::pair<pstring, pstring>> cTargetsSameFileAndNameCheck;
    void initializeCTarget();

  public:
    pstring name;
    // If target has file, this is that file directory. Else, it is the directory of container it is present in.
    pstring targetFileDir;

    pstring targetSubDir;

    set<CTarget *, CTargetPointerComparator> elements;

    Json json;
    inline static size_t total = 0;
    size_t id = 0; // unique for every BTarget

    // If constructor with other target is used, the pointer to the other target
    CTarget *other = nullptr;
    // This points to the tarjanNodeCTargets set element
    TCT *cTarjanNode = nullptr;
    const bool hasFile = true;
    bool selectiveBuildSet = false;
    bool callPreSort = true;

  private:
    bool selectiveBuild = false;

  public:
    CTarget(pstring name_, CTarget &container, bool hasFile_ = true);
    explicit CTarget(pstring name_);
    virtual ~CTarget();
    pstring getTargetPointer() const;
    path getTargetFilePath() const;
    bool getSelectiveBuild();
    bool getSelectiveBuildChildDir();

    virtual pstring getTarjanNodeName() const;
    virtual void setJson();
    virtual void writeJsonFile();
    virtual void configure();
    virtual BTarget *getBTarget();
    virtual C_Target *get_CAPITarget(BSMode bsMode);
};
void to_json(Json &j, const CTarget *tar);
bool operator<(const CTarget &lhs, const CTarget &rhs);

#endif // HMAKE_BASICTARGETS_HPP
