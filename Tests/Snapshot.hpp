
#ifndef HMAKE_SNAPSHOT_HPP
#define HMAKE_SNAPSHOT_HPP

#include "SMFile.hpp"
#include <filesystem>

using std::filesystem::path, std::filesystem::file_time_type, std::filesystem::current_path;

struct NodeCompare
{
    bool operator()(const Node *lhs, const Node *rhs) const;
};

struct Test2Touched
{
    bool appLinked = false;
    bool mainDotCpp = false;
    bool lib1Linked = false;
    bool lib1DotCpp = false;
    bool privateLib1DotHpp = false;
    bool publicLib1DotHpp = false;
    bool lib2Linked = false;
    bool lib2DotCpp = false;
    bool privateLib2DotHpp = false;
    bool publicLib2DotHpp = false;
    bool lib3Linked = false;
    bool lib3DotCpp = false;
    bool privateLib3DotHpp = false;
    bool publicLib3DotHpp = false;
    bool lib4Linked = false;
    bool lib4DotCpp = false;
    bool privateLib4DotHpp = false;
    bool publicLib4DotHpp = false;
};

struct Test1Setup
{
    const path &hbuildExecutionPath = current_path();
    const path &snapshotPath = current_path();
    bool sourceFileUpdated = false;
    bool executableFileUpdated = false;
};

class Snapshot
{
    set<Node> beforeData;
    set<Node> afterData;

  public:
    explicit Snapshot(const path &directoryPath);
    void before(const path &directoryPath);
    void after(const path &directoryPath);
    bool snapshotBalancesTest1(bool sourceFileUpdated, bool executableUpdated);
    bool snapshotBalances(unsigned short filesCompiled, unsigned short cppTargets, unsigned short linkTargetsNoDebug,
                          unsigned short linkTargetsDebug);
    bool snapshotBalancesTest2(Test2Touched touched);
};

#endif // HMAKE_SNAPSHOT_HPP
