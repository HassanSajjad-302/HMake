#include "Snapshot.hpp"
#include "BuildSystemFunctions.hpp"

using std::filesystem::recursive_directory_iterator;

bool NodeCompare::operator()(const Node *lhs, const Node *rhs) const
{
    return *lhs < *rhs;
}

Snapshot::Snapshot(const path &directoryPath)
{
    before(directoryPath);
}

void Snapshot::before(const path &directoryPath)
{
    beforeData.clear();
    afterData.clear();
    Node::allFiles.clear();
    for (auto &f : recursive_directory_iterator(directoryPath))
    {
        if (f.is_regular_file())
        {
            Node *node = const_cast<Node *>(Node::getNodeFromString(f.path().string(), true));
            node->getLastUpdateTime();
            beforeData.emplace(*node);
        }
    }
}

void Snapshot::after(const path &directoryPath)
{
    Node::allFiles.clear();
    for (auto &f : recursive_directory_iterator(directoryPath))
    {
        if (f.is_regular_file())
        {
            Node *node = const_cast<Node *>(Node::getNodeFromString(f.path().string(), true));
            node->getLastUpdateTime();
            afterData.emplace(*node);
        }
    }
}

bool Snapshot::snapshotBalances(const Updates &updates)
{
    set<const Node *> difference;
    for (const Node &node : afterData)
    {
        if (!beforeData.contains(node) || beforeData.find(node)->getLastUpdateTime() != node.getLastUpdateTime())
        {
            difference.emplace(&node);
        }
    }
    unsigned short sum = 0;
    unsigned short debugLinkTargetsMultiplier = os == OS::NT ? 6 : 3; // No response file on Linux
    unsigned short noDebugLinkTargetsMultiplier = os == OS::NT ? 4 : 3;

    // Output, Error, .smrules, Respone File on Windows / Deps Output File on Linux
    sum += 4 * updates.smruleFiles;
    // Output, Error, .o, Respone File on Windows / Deps Output File on Linux
    sum += 4 * updates.sourceFiles;

    sum += 3 * updates.errorFiles;
    sum += 5 * updates.moduleFiles;

    sum += updates.linkTargetsNoDebug * noDebugLinkTargetsMultiplier;
    sum += updates.linkTargetsDebug * debugLinkTargetsMultiplier;
    if (updates.cppTargets || updates.linkTargetsNoDebug || updates.linkTargetsDebug)
    {
        sum += 1; // build-cache.jon
    }
    if (difference.size() != sum)
    {
        bool breakpoint = true;
    }
    return difference.size() == sum;
}