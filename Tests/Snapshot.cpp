#include "Snapshot.hpp"
#include "BuildSystemFunctions.hpp"
#include <utility>

using std::filesystem::recursive_directory_iterator;

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
    Node::clearNodes();
    for (auto &f : recursive_directory_iterator(directoryPath))
    {
        if (f.is_regular_file())
        {
            Node *node = Node::getNodeFromNonNormalizedPath(f.path(), true);
            beforeData.emplace(node->filePath, node->lastWriteTime);
        }
    }
}

void Snapshot::after(const path &directoryPath)
{
    Node::clearNodes();
    for (auto &f : recursive_directory_iterator(directoryPath))
    {
        if (f.is_regular_file())
        {
            Node *node = Node::getNodeFromNonNormalizedPath(f.path(), true);
            node->lastWriteTime = last_write_time(path(node->filePath));
            afterData.emplace(node->filePath, node->lastWriteTime);
        }
    }
}

bool Snapshot::snapshotBalances(const Updates &updates) const
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
    constexpr unsigned short debugLinkTargetsMultiplier = os == OS::NT ? 6 : 3; // No response file on Linux
    constexpr unsigned short noDebugLinkTargetsMultiplier = os == OS::NT ? 4 : 3;

    // Output, Error, .smrules, Respone File on Windows / Deps Output File on Linux
    expected += 4 * updates.smruleFiles;
    // Output, Error, .o, Respone File on Windows / Deps Output File on Linux
    expected += 4 * updates.sourceFiles;

    expected += 3 * updates.errorFiles;
    expected += 5 * updates.moduleFiles;

    expected += updates.linkTargetsNoDebug * noDebugLinkTargetsMultiplier;
    expected += updates.linkTargetsDebug * debugLinkTargetsMultiplier;

    if (updates.nodesFile)
    {
        // nodes.json
        expected += 1;
    }

    if (updates.cppTargets || updates.linkTargetsNoDebug || updates.linkTargetsDebug)
    {
        expected += 1; // build-cache.json
    }
    if (actual.size() != expected)
    {
        bool breakpoint = true;
    }
    return actual.size() == expected;
}