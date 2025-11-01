
#ifndef HMAKE_SNAPSHOT_HPP
#define HMAKE_SNAPSHOT_HPP

#include "parallel-hashmap/parallel_hashmap/phmap.h"
#include <filesystem>

using std::filesystem::path, std::filesystem::file_time_type, std::filesystem::current_path, phmap::flat_hash_set;

struct Setup
{
    const path &hbuildExecutionPath = current_path();
    unsigned short filesCompiled;
    unsigned short cppTargets;
    unsigned short linkTargetsNoDebug;
    unsigned short linkTargetsDebug;
};

struct Test1Setup
{
    const path &hbuildExecutionPath = current_path();
    bool sourceFileUpdated = false;
    bool executableFileUpdated = false;
};

struct Updates
{
    unsigned short errorFiles = 0;
    unsigned short headerUnits = 0;
    // module-files which don't have .ifc file generated are also considered sourceFiles
    unsigned short sourceFiles = 0;
    unsigned short moduleFiles = 0;
    unsigned short imodFiles = 0;
    unsigned short linkTargetsNoDebug = 0;
    unsigned short linkTargetsDebug = 0;
    bool nodesFile = false;
};

struct NodeSnap
{
    path nodePath;
    file_time_type lastUpdateTime;
    NodeSnap(path nodePath_, file_time_type time_);
};
uint64_t hash_value(const NodeSnap &p);
bool operator==(const NodeSnap &lhs, const NodeSnap &rhs);

class Snapshot
{
    flat_hash_set<NodeSnap> beforeData;
    flat_hash_set<NodeSnap> afterData;

  public:
    explicit Snapshot(const path &dirPath);
    void before(const path &dirPath);
    void after(const path &dirPath);
    bool snapshotBalances(const Updates &updates) const;
};

#endif // HMAKE_SNAPSHOT_HPP
