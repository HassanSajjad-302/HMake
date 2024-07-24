#include "Snapshot.hpp"

#include "BuildSystemFunctions.hpp"
#include <utility>

using std::filesystem::recursive_directory_iterator;

bool NodeCompare::operator()(const Node *lhs, const Node *rhs) const
{
    return *lhs < *rhs;
}

NodeSnap::NodeSnap(path nodePath_, const file_time_type time_) : nodePath{std::move(nodePath_)}, lastUpdateTime{time_}
{
}

bool operator<(const NodeSnap &lhs, const NodeSnap &rhs)
{
    return lhs.nodePath < rhs.nodePath;
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
            Node *node = const_cast<Node *>(Node::getNodeFromNonNormalizedPath(f.path(), true));
            beforeData.emplace(node->filePath, node->getLastUpdateTime());
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
            Node *node = const_cast<Node *>(Node::getNodeFromNonNormalizedPath(f.path(), true));
            afterData.emplace(node->filePath, node->getLastUpdateTime());
        }
    }
}

bool Snapshot::snapshotBalances(const Updates &updates)
{
    set<const NodeSnap *> actual;
    for (const NodeSnap &snap : afterData)
    {
        if (!beforeData.contains(snap) || beforeData.find(snap)->lastUpdateTime != snap.lastUpdateTime)
        {
            actual.emplace(&snap);
        }
    }
    unsigned short expected = 0;
    const unsigned short debugLinkTargetsMultiplier = os == OS::NT ? 6 : 3; // No response file on Linux
    const unsigned short noDebugLinkTargetsMultiplier = os == OS::NT ? 4 : 3;

    // Output, Error, .smrules, Respone File on Windows / Deps Output File on Linux
    expected += 4 * updates.smruleFiles;
    // Output, Error, .o, Respone File on Windows / Deps Output File on Linux
    expected += 4 * updates.sourceFiles;

    expected += 3 * updates.errorFiles;
    expected += 5 * updates.moduleFiles;

    expected += updates.linkTargetsNoDebug * noDebugLinkTargetsMultiplier;
    expected += updates.linkTargetsDebug * debugLinkTargetsMultiplier;
    if (updates.cppTargets || updates.linkTargetsNoDebug || updates.linkTargetsDebug)
    {
        expected += 1; // build-cache.jon
    }
    if (actual.size() != expected)
    {
        bool breakpoint = true;
    }
    return actual.size() == expected;
}