
#ifndef HMAKE_SNAPSHOT_HPP
#define HMAKE_SNAPSHOT_HPP

#include "SMFile.hpp"
#include <filesystem>

using std::filesystem::path, std::filesystem::file_time_type;

struct NodeCompare
{
    bool operator()(const Node *lhs, const Node *rhs) const;
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
};

#endif // HMAKE_SNAPSHOT_HPP
