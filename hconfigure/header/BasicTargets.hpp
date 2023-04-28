#ifndef HMAKE_BASICTARGETS_HPP
#define HMAKE_BASICTARGETS_HPP
#ifdef USE_HEADER_UNITS
import "C_API.hpp";
import "TargetType.hpp";
import "TarjanNode.hpp";
import <filesystem>;
import <map>;
#else
#include "C_API.hpp"
#include "TargetType.hpp"
#include "TarjanNode.hpp"
#include <filesystem>
#include <map>
#endif

using std::filesystem::path, std::size_t, std::map;

// TBT = TarjanNodeBTarget    TCT = TarjanNodeCTarget
TarjanNode(const struct BTarget *) -> TarjanNode<BTarget>;
using TBT = TarjanNode<BTarget>;
inline map<unsigned short, set<TBT>> tarjanNodesBTargets;

TarjanNode(const class CTarget *) -> TarjanNode<CTarget>;
using TCT = TarjanNode<CTarget>;
inline set<TCT> tarjanNodesCTargets;

struct RealBTarget;
struct IndexInTopologicalSortComparatorRoundZero
{
    bool operator()(const BTarget *lhs, const BTarget *rhs) const;
};

enum class FileStatus
{
    NOT_ASSIGNED,
    UPDATED,
    NEEDS_UPDATE
};

struct RealBTarget
{
    set<BTarget *> dependents;
    set<BTarget *> dependencies;
    unsigned int indexInTopologicalSort = 0;
    unsigned int dependenciesSize = 0;
    FileStatus fileStatus = FileStatus::UPDATED;
    // This points to the tarjanNodeBTargets set element
    TBT *bTarjanNode;
    BTarget *bTarget;
    bool dependencyNeedsUpdate = false;
    bool updateCalled = false;
    // Plays two roles. Depicts the exitStatus of itself and of its dependencies
    int exitStatus = EXIT_SUCCESS;
    unsigned short round;
    explicit RealBTarget(BTarget *bTarget_, unsigned short round);

    template <typename... U> void addDependency(BTarget &dependency, U &...bTargets);
    // TODO
    //  Following describes the time taken for the completion of this task. Currently unused. how to determine cpu time
    //  for this task in multi-threaded scenario
    unsigned long timeTaken = 0;

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
};

namespace BTargetNamespace
{
inline std::mutex addDependencyMutex;
}

struct BTarget // BTarget
{
    inline static size_t total = 0;
    size_t id = 0; // unique for every BTarget
    bool selectiveBuild = false;

    map<unsigned short, RealBTarget> realBTargets;
    explicit BTarget();
    virtual ~BTarget();

    virtual string getTarjanNodeName() const;

    RealBTarget &getRealBTarget(unsigned short round);
    virtual void preSort(class Builder &builder, unsigned short round);
    virtual void duringSort(Builder &builder, unsigned short round);
    virtual void updateBTarget(Builder &builder, unsigned short round);

    // TODO
    // Following describes total time taken across all rounds. i.e. sum of all RealBTarget::timeTaken.
    float totalTimeTaken = 0.0f;
};
bool operator<(const BTarget &lhs, const BTarget &rhs);

template <typename... U> void RealBTarget::addDependency(BTarget &dependency, U &...bTargets)
{
    if (dependencies.emplace(&dependency).second)
    {
        RealBTarget &dependencyRealBTarget = dependency.getRealBTarget(round);
        dependencyRealBTarget.dependents.emplace(bTarget);
        ++dependenciesSize;
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
    inline static set<std::pair<string, string>> cTargetsSameFileAndNameCheck;
    void initializeCTarget();
    bool selectiveBuild = false;

  public:
    string name;
    Json json;
    set<CTarget *, CTargetPointerComparator> elements;
    inline static size_t total = 0;
    size_t id = 0; // unique for every BTarget
    // If target has file, this is that file directory. Else, it is the directory of container it is present in.
    string targetFileDir;
    // If constructor with other target is used, the pointer to the other target
    CTarget *other = nullptr;
    // This points to the tarjanNodeCTargets set element
    TCT *cTarjanNode = nullptr;
    const bool hasFile = true;
    bool selectiveBuildSet = false;
    bool callPreSort = true;
    CTarget(string name_, CTarget &container, bool hasFile_ = true);
    explicit CTarget(string name_);
    virtual ~CTarget();
    string getTargetPointer() const;
    path getTargetFilePath() const;
    string getSubDirForTarget() const;
    bool getSelectiveBuild();

    virtual string getTarjanNodeName() const;
    virtual void setJson();
    virtual void writeJsonFile();
    virtual void configure();
    virtual BTarget *getBTarget();
    virtual C_Target *get_CAPITarget(BSMode bsMode);
};
void to_json(Json &j, const CTarget *tar);
bool operator<(const CTarget &lhs, const CTarget &rhs);

#endif // HMAKE_BASICTARGETS_HPP
