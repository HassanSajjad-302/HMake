
#ifndef HMAKE_SNAPSHOT_HPP
#define HMAKE_SNAPSHOT_HPP

#include "SMFile.hpp"
#include <filesystem>

using std::filesystem::path, std::filesystem::file_time_type;

struct NodeCompare
{
    bool operator()(const Node *lhs, const Node *rhs) const;
};

struct Snapshot
{
    explicit Snapshot(const path &directoryPath);
    set<Node> data;
    static bool snapshotBalances(const Snapshot &before, const Snapshot &after, unsigned short sourceFileTargets,
                                 unsigned short linkTargets, unsigned short cacheFileTargets);
};

#endif // HMAKE_SNAPSHOT_HPP
