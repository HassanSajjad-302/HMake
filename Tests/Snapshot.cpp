#include "Snapshot.hpp"
#include "BuildSystemFunctions.hpp"

using std::filesystem::recursive_directory_iterator;

bool NodeCompare::operator()(const Node *lhs, const Node *rhs) const
{
    return *lhs < *rhs;
}

Snapshot::Snapshot(const path &directoryPath)
{
    Node::allFiles.clear();
    for (auto &f : recursive_directory_iterator(directoryPath))
    {
        if (f.is_regular_file())
        {
            Node *node = const_cast<Node *>(Node::getNodeFromString(f.path().generic_string(), true));
            node->getLastUpdateTime();
            data.emplace(*node);
        }
    }
}
bool Snapshot::snapshotBalances(const Snapshot &before, const Snapshot &after, unsigned short sourceFileTargets,
                                unsigned short linkTargets, unsigned short cacheFileTargets)
{
    set<const Node *> difference;
    for (const Node &node : after.data)
    {
        if (!before.data.contains(node) || before.data.find(node)->getLastUpdateTime() != node.getLastUpdateTime())
        {
            difference.emplace(&node);
        }
    }
    // On Linux, there is no response file but for sourceFileTargets, there is header-dependency file, while
    // cacheFileTargets are those for which there is also a cache file while for nonCacheTargets there is only
    // target.json
    unsigned short linkTargetMultiplier = os == OS::NT ? 4 : 3;
    unsigned short sum = (sourceFileTargets * 4) + (linkTargets * linkTargetMultiplier) + (cacheFileTargets * 1);
    return difference.size() == sum;
}