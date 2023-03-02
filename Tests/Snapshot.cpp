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
            Node *node = const_cast<Node *>(Node::getNodeFromString(f.path().generic_string(), true));
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
            Node *node = const_cast<Node *>(Node::getNodeFromString(f.path().generic_string(), true));
            node->getLastUpdateTime();
            afterData.emplace(*node);
        }
    }
}

bool Snapshot::snapshotBalancesTest1(bool sourceFileUpdated, bool executableUpdated)
{
    set<const Node *> difference;
    for (const Node &node : afterData)
    {
        if (!beforeData.contains(node) || beforeData.find(node)->getLastUpdateTime() != node.getLastUpdateTime())
        {
            difference.emplace(&node);
        }
    }
    unsigned short linkTargetMultiplier = os == OS::NT ? 4 : 3;
    unsigned short sum = 0;
    if (sourceFileUpdated)
    {
        // Output, Error, .o, Respone File on Windows / Deps Output File on Linux, CppSourceTarget Cache File
        sum += 5;
        executableUpdated = true;
    }
    if (executableUpdated)
    {
        if constexpr (os == OS::NT)
        {
            // Output, Error, Response, LinkOrArchiveTarget Cache File, EXE, PDB, ILK
            sum += 7;
        }
        else
        {
            // Output, Error, LinkOrArchiveTarget Cache File, EXE
            sum += 4;
        }
    }
    return difference.size() == sum;
}