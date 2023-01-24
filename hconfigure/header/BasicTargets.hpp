#ifndef HMAKE_BASICTARGETS_HPP
#define HMAKE_BASICTARGETS_HPP

#include "TargetType.hpp"
#include "TarjanNode.hpp"
#include <filesystem>

using std::filesystem::path;

// TBT = TarjanNodeBTarget    TCT = TarjanNodeCTarget
TarjanNode(const struct BTarget *) -> TarjanNode<BTarget>;
using TBT = TarjanNode<BTarget>;
inline set<TBT> tarjanNodesBTargets;

TarjanNode(const struct CTarget *) -> TarjanNode<CTarget>;
using TCT = TarjanNode<CTarget>;
inline set<TCT> tarjanNodesCTargets;

struct IndexInTopologicalSortComparator
{
    bool operator()(const BTarget *lhs, const BTarget *rhs) const;
};

enum class ResultType
{
    LINK,
    SOURCENODE,
    CPP_SMFILE,
    CPP_MODULE,
    ACTIONTARGET,
};

enum class FileStatus
{
    NOT_ASSIGNED,
    UPDATED,
    NEEDS_UPDATE
};

struct BTarget // BTarget
{
    vector<BTarget *> sameTypeDependents;
    set<BTarget *> dependents;
    set<BTarget *> dependencies;
    // This includes dependencies and their dependencies arranged on basis of indexInTopologicalSort.
    set<BTarget *, IndexInTopologicalSortComparator> allDependencies;
    inline static unsigned long total = 0;
    unsigned long id = 0; // unique for every BTarget
    unsigned long topologicallySortedId = 0;
    unsigned int dependenciesSize = 0;
    ResultType resultType;
    TargetType bTargetType;
    FileStatus fileStatus = FileStatus::UPDATED;
    // This points to the tarjanNodeBTargets set element
    TBT *bTarjanNode = nullptr;
    // Value is assigned on basis of TBT::topologicalSort index. Targets in allDependencies vector are arranged by this
    // value.
    unsigned long indexInTopologicalSort = 0;
    explicit BTarget(ResultType resultType_);
    void addDependency(BTarget &dependency);
    virtual void updateBTarget();
    virtual void printMutexLockRoutine();
    virtual void initializeForBuild();
    virtual void checkForPreBuiltAndCacheDir();
    virtual void parseModuleSourceFiles(class Builder &builder);
    virtual void checkForHeaderUnitsCache();
    virtual void createHeaderUnits();
    virtual void populateSetTarjanNodesSourceNodes(class Builder &builder);
};
bool operator<(const BTarget &lhs, const BTarget &rhs);

struct TarPointerComparator
{
    bool operator()(const struct CTarget *lhs, const struct CTarget *rhs) const;
};

template <typename T>
concept SetOrVectorOfCTargetPointer =
    requires(T a) {
        a.begin();
        a.end();
        std::derived_from<std::remove_pointer<typename decltype(a)::value_type>, CTarget>;
    };

struct CTargetPointerComparator
{
    bool operator()(const CTarget *lhs, const CTarget *rhs) const;
};

class CTarget // Configure Target
{
    inline static set<CTarget *, CTargetPointerComparator> containerCTargets;
    void initializeCTarget();

  public:
    string name;
    Json json;
    set<CTarget *, TarPointerComparator> elements;
    inline static unsigned long total = 0;
    unsigned long id = 0; // unique for every BTarget
    // If target has file, this is that file directory. Else, it is the directory of container it is present in.
    string targetFileDir;
    // If constructor with other target is used, the pointer to the other target
    CTarget *other = nullptr;
    // This points to the tarjanNodeCTargets set element
    TCT *cTarjanNode = nullptr;
    TargetType cTargetType = TargetType::NOT_ASSIGNED;
    const bool hasFile = true;
    // addNodeInTarjanNodeTargets might be called more than once in BSMode::BUILD
    bool addNodeInTarjanNodeTargetsCalled = false;
    CTarget(string name_, CTarget &container, bool hasFile_ = true);
    explicit CTarget(string name_);
    string getTargetPointer() const;
    path getTargetFilePath() const;
    template <SetOrVectorOfCTargetPointer T> void addCTargetDependency(T &cTarget);
    virtual void setJson();
    virtual void writeJsonFile();
    virtual void configure();
    virtual BTarget *getBTarget();
    // Following provides configure-time checks for dependency cycle between CTargets.  This is different from
    // container-element dependency. Container-Element dependency-cycle is only checked in configure mode while
    // this is also checked in build mode. This is used to check for cyclic dependencies between libraries.
    virtual void populateCTargetDependencies();
    virtual void addPrivatePropertiesToPublicProperties();
};
void to_json(Json &j, const CTarget *tar);

template <SetOrVectorOfCTargetPointer T> void CTarget::addCTargetDependency(T &containerOfPointers)
{
    for (CTarget *cTarget : containerOfPointers)
    {
        cTarjanNode->deps.emplace(cTarget->cTarjanNode);
    }
}

#endif // HMAKE_BASICTARGETS_HPP
