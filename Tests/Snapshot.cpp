#include "Snapshot.hpp"
#include "BuildSystemFunctions.hpp"
#include "rapidhash/rapidhash.h"
#include <iostream>
#include <stacktrace>
#include <utility>

using std::filesystem::recursive_directory_iterator;

NodeSnap::NodeSnap(path nodePath_, const file_time_type time_) : nodePath{std::move(nodePath_)}, lastUpdateTime{time_}
{
}

consteval uint64_t pathCharSize()
{
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

Snapshot::Snapshot(const path &dirPath)
{
    before(dirPath);
}

void Snapshot::before(const path &dirPath)
{
    beforeData.clear();
    afterData.clear();
    for (auto &f : recursive_directory_iterator(dirPath))
    {
        if (f.is_regular_file())
        {
            beforeData.emplace(f.path(), last_write_time(f));
        }
    }
}

void Snapshot::after(const path &dirPath)
{
    for (auto &f : recursive_directory_iterator(dirPath))
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
    constexpr unsigned short debugLinkTargetsMultiplier = os == OS::NT ? 3 : 1;
    constexpr unsigned short noDebugLinkTargetsMultiplier = 1;

    // .smrules, on Windows / Deps Output File on Linux
    expected += (os == OS::NT ? 1 : 2) * updates.smruleFiles;
    // .o, on Windows / Deps Output File on Linux
    expected += (os == OS::NT ? 1 : 2) * updates.sourceFiles;

    // expected += 3 * updates.errorFiles;
    expected += 2 * updates.moduleFiles;

    expected += (os == OS::NT ? 0 : 1) * updates.errorFiles;

    expected += updates.linkTargetsNoDebug * noDebugLinkTargetsMultiplier;
    expected += updates.linkTargetsDebug * debugLinkTargetsMultiplier;

    if (updates.nodesFile)
    {
        // nodes.json
        expected += 1;
    }

    if (updates.sourceFiles || updates.moduleFiles || updates.smruleFiles || updates.linkTargetsNoDebug ||
        updates.linkTargetsDebug)
    {
        expected += 1; // build-cache.json
    }

    if (actual.size() != expected)
    {
        bool breakpoint = true;
        printMessage(FORMAT("Actual {}\tExpected {}\n", actual.size(), expected));

        for (const NodeSnap *nodeSnap : actual)
        {
            printMessage(nodeSnap->nodePath.string() + '\n');
        }
        breakpoint = true;
        std::cout << std::stacktrace::current() << std::endl;
    }
    return actual.size() == expected;
}