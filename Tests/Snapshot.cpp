#include "Snapshot.hpp"
#include "BuildSystemFunctions.hpp"
#include "rapidhash.h"
#include <utility>

using std::filesystem::recursive_directory_iterator;

NodeSnap::NodeSnap(path nodePath_, const file_time_type time_) : nodePath{std::move(nodePath_)}, lastUpdateTime{time_}
{
}

consteval uint64_t pathCharSize()
{
    path p;
    if constexpr (std::is_same<path::string_type, string>::value)
    {
        return 1;
    }
    return 2;
}

uint64_t hash_value(const NodeSnap &p)
{
    return rapidhash(p.nodePath.native().c_str(), pathCharSize() * p.nodePath.native().size());
}

bool operator==(const NodeSnap &lhs, const NodeSnap &rhs)
{
    return lhs.nodePath == rhs.nodePath;
}

Snapshot::Snapshot(const path &directoryPath)
{
    before(directoryPath);
}

void Snapshot::before(const path &directoryPath)
{
    beforeData.clear();
    afterData.clear();
    for (auto &f : recursive_directory_iterator(directoryPath))
    {
        if (f.is_regular_file())
        {
            beforeData.emplace(f.path(), last_write_time(f));
        }
    }
}

void Snapshot::after(const path &directoryPath)
{
    for (auto &f : recursive_directory_iterator(directoryPath))
    {
        if (f.is_regular_file())
        {
            afterData.emplace(f.path(), last_write_time(f));
        }
    }
}

bool Snapshot::snapshotBalances(const Updates &updates) const
{
    flat_hash_set<const NodeSnap *> actual;
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