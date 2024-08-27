
#ifndef HMAKE_SNAPSHOT_HPP
#define HMAKE_SNAPSHOT_HPP

#include "Node.hpp"
#include <filesystem>

using std::filesystem::path, std::filesystem::file_time_type, std::filesystem::current_path;

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
    unsigned short smruleFiles = 0;
    // module-files which don't have .ifc file generated are also considered sourceFiles
    unsigned short sourceFiles = 0;
    unsigned short moduleFiles = 0;
    unsigned short cppTargets = 0;
    unsigned short linkTargetsNoDebug = 0;
    unsigned short linkTargetsDebug = 0;
    bool nodesFile = true;
};

struct NodeSnap
{
    path nodePath;
    file_time_type lastUpdateTime;
    NodeSnap(path nodePath_, file_time_type time_);
};
bool operator<(const NodeSnap &lhs, const NodeSnap &rhs);

class Snapshot
{
    set<NodeSnap> beforeData;
    set<NodeSnap> afterData;

  public:
    explicit Snapshot(const path &directoryPath);
    void before(const path &directoryPath);
    void after(const path &directoryPath);
    bool snapshotBalances(const Updates &updates) const;
};

#endif // HMAKE_SNAPSHOT_HPP
