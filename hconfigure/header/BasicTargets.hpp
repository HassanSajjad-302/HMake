#ifndef HMAKE_BASICTARGETS_HPP
#define HMAKE_BASICTARGETS_HPP

#include "TargetType.hpp"
#include "TarjanNode.hpp"
#include <filesystem>
#include <map>

using std::filesystem::path, std::size_t, std::map;

// TBT = TarjanNodeBTarget    TCT = TarjanNodeCTarget
TarjanNode(const struct BTarget *) -> TarjanNode<BTarget>;
using TBT = TarjanNode<BTarget>;
inline map<unsigned short, set<TBT>> tarjanNodesBTargets;

TarjanNode(const class CTarget *) -> TarjanNode<CTarget>;
using TCT = TarjanNode<CTarget>;
inline set<TCT> tarjanNodesCTargets;

struct RealBTarget;
struct IndexInTopologicalSortComparator
{
    explicit IndexInTopologicalSortComparator(unsigned short round_);
    bool operator()(const BTarget *lhs, const BTarget *rhs) const;
    unsigned short round;
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
    // This includes dependencies and their dependencies arranged on basis of indexInTopologicalSort.
    set<BTarget *, IndexInTopologicalSortComparator> allDependencies;
    unsigned int dependenciesSize = 0;
    FileStatus fileStatus = FileStatus::UPDATED;
    // This points to the tarjanNodeBTargets set element
    TBT *bTarjanNode;
    // Value is assigned on basis of TBT::topologicalSort index. Targets in allDependencies vector are arranged by this
    // value.
    size_t indexInTopologicalSort = 0;
    BTarget *bTarget;
    int dependenciesExitStatus = EXIT_SUCCESS;
    int exitStatus = EXIT_SUCCESS;
    unsigned short round;
    explicit RealBTarget(unsigned short round_, BTarget *bTarget_);
    void addDependency(BTarget &dependency);
};

struct CompareRealBTargetId
{
    using is_transparent = void; // for example with void,
                                 // but could be int or struct CanSearchOnId;
    bool operator()(RealBTarget const &lhs, RealBTarget const &rhs) const;
    bool operator()(unsigned short round, RealBTarget const &rhs) const;
    bool operator()(RealBTarget const &lhs, unsigned short round) const;
};
bool operator<(const RealBTarget &lhs, const RealBTarget &rhs);

namespace BTargetNamespace
{
inline std::mutex addDependencyMutex;
}

struct BTarget // BTarget
{
    inline static size_t total = 0;
    size_t id = 0; // unique for every BTarget
    bool selectiveBuild = false;

    set<RealBTarget, CompareRealBTargetId> realBTargets;
    explicit BTarget();

    virtual string getTarjanNodeName();

    RealBTarget &getRealBTarget(unsigned short round);
    virtual void updateBTarget(unsigned short round, class Builder &builder);
    virtual void printMutexLockRoutine(unsigned short round);
    virtual void preSort(Builder &builder, unsigned short round);
    virtual void duringSort(Builder &builder, unsigned short round);
};
bool operator<(const BTarget &lhs, const BTarget &rhs);

struct TarPointerComparator
{
    bool operator()(const CTarget *lhs, const CTarget *rhs) const;
};

struct CTargetPointerComparator
{
    bool operator()(const CTarget *lhs, const CTarget *rhs) const;
};

class CTarget // Configure Target
{
    inline static set<CTarget *, CTargetPointerComparator> cTargetsSameFileAndNameCheck;
    void initializeCTarget();

  public:
    string name;
    Json json;
    set<CTarget *, TarPointerComparator> elements;
    inline static size_t total = 0;
    size_t id = 0; // unique for every BTarget
    // If target has file, this is that file directory. Else, it is the directory of container it is present in.
    string targetFileDir;
    // If constructor with other target is used, the pointer to the other target
    CTarget *other = nullptr;
    // This points to the tarjanNodeCTargets set element
    TCT *cTarjanNode = nullptr;
    const bool hasFile = true;
    CTarget(string name_, CTarget &container, bool hasFile_ = true);
    explicit CTarget(string name_);
    string getTargetPointer() const;
    path getTargetFilePath() const;
    string getSubDirForTarget() const;

    virtual string getTarjanNodeName();
    virtual void setJson();
    virtual void writeJsonFile();
    virtual void configure();
    virtual BTarget *getBTarget();
};
void to_json(Json &j, const CTarget *tar);

#endif // HMAKE_BASICTARGETS_HPP
