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
    unsigned short debugLinkTargetsMultiplier = os == OS::NT ? 7 : 4; // No response file on Linux
    unsigned short noDebugLinkTargetsMultiplier = os == OS::NT ? 5 : 4;

    // Output, Error, .smrules, Respone File on Windows / Deps Output File on Linux
    sum += 4 * updates.smruleFiles;
    // Output, Error, .o, Respone File on Windows / Deps Output File on Linux
    sum += 4 * updates.sourceFiles;

    sum += 3 * updates.errorFiles;
    sum += 5 * updates.moduleFiles;

    sum += updates.linkTargetsNoDebug * noDebugLinkTargetsMultiplier;
    sum += updates.linkTargetsDebug * debugLinkTargetsMultiplier;
    sum += updates.cppTargets;
    if (difference.size() != sum)
    {
        bool breakpoint = true;
    }
    return difference.size() == sum;
}

bool Snapshot::snapshotBalances(unsigned short smruleFiles, unsigned short filesCompiled, unsigned short cppTargets,
                                unsigned short linkTargetsNoDebug, unsigned short linkTargetsDebug)
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
    unsigned short debugLinkTargetsMultiplier = os == OS::NT ? 7 : 4; // No response file on Linux
    unsigned short noDebugLinkTargetsMultiplier = os == OS::NT ? 5 : 4;

    // Output, Error, .smrules, Respone File on Windows / Deps Output File on Linux
    sum += 4 * smruleFiles;
    // Output, Error, .o, Respone File on Windows / Deps Output File on Linux
    sum += 4 * filesCompiled;

    sum += linkTargetsNoDebug * noDebugLinkTargetsMultiplier;
    sum += linkTargetsDebug * debugLinkTargetsMultiplier;
    sum += cppTargets;
    if (difference.size() != sum)
    {
        bool breakpoint = true;
    }
    return difference.size() == sum;
}

bool Snapshot::snapshotErroneousBalances(unsigned short errorFiles, unsigned short smruleFiles,
                                         unsigned short filesCompiled, unsigned short cppTargets,
                                         unsigned short linkTargetsNoDebug, unsigned short linkTargetsDebug)
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
    unsigned short debugLinkTargetsMultiplier = os == OS::NT ? 7 : 4; // No response file on Linux
    unsigned short noDebugLinkTargetsMultiplier = os == OS::NT ? 5 : 4;

    // Output, Error, Respone File on Windows / Deps Output File on Linux
    sum += 3 * errorFiles;

    // Output, Error, .smrules, Respone File on Windows / Deps Output File on Linux
    sum += 4 * smruleFiles;

    // Output, Error, .o, Respone File on Windows / Deps Output File on Linux
    sum += 4 * filesCompiled;

    sum += linkTargetsNoDebug * noDebugLinkTargetsMultiplier;
    sum += linkTargetsDebug * debugLinkTargetsMultiplier;
    sum += cppTargets;
    if (difference.size() != sum)
    {
        bool breakpoint = true;
    }
    return difference.size() == sum;
}

bool Snapshot::snapshotBalancesTest2(Test2Setup touched)
{
    if (touched.publicLib4DotHpp)
    {
        touched.lib4DotCpp = true;
        touched.lib3DotCpp = true;
        touched.lib2DotCpp = true;
    }

    if (touched.privateLib4DotHpp)
    {
        touched.lib4DotCpp = true;
    }

    if (touched.publicLib3DotHpp)
    {
        touched.lib3DotCpp = true;
        touched.lib2DotCpp = true;
    }

    if (touched.privateLib3DotHpp)
    {
        touched.lib3DotCpp = true;
    }

    if (touched.publicLib2DotHpp)
    {
        touched.lib2DotCpp = true;
        touched.lib1DotCpp = true;
        touched.mainDotCpp = true;
    }

    if (touched.privateLib2DotHpp)
    {
        touched.lib2DotCpp = true;
    }

    if (touched.publicLib1DotHpp)
    {
        touched.lib1DotCpp = true;
        touched.mainDotCpp = true;
    }

    if (touched.privateLib1DotHpp)
    {
        touched.lib1DotCpp = true;
    }

    if (touched.lib1DotCpp)
    {
        touched.lib1Linked = true;
    }
    if (touched.lib2DotCpp)
    {
        touched.lib2Linked = true;
    }
    if (touched.lib3DotCpp)
    {
        touched.lib3Linked = true;
    }
    if (touched.lib4DotCpp)
    {
        touched.lib4Linked = true;
    }

    if (touched.lib1Linked || touched.lib2Linked || touched.lib3Linked || touched.lib4Linked || touched.mainDotCpp)
    {
        touched.appLinked = true;
    }

    unsigned short linkTargets = 0;
    unsigned short linkTargetsDebug = 0;
    unsigned short cppTargets = 0;
    unsigned short filesCompiled = 0;

    if (touched.appLinked)
    {
        ++linkTargetsDebug;
    }
    if (touched.mainDotCpp)
    {
        ++cppTargets;
        ++filesCompiled;
    }
    if (touched.lib1Linked)
    {
        ++linkTargets;
    }
    if (touched.lib1DotCpp)
    {
        ++cppTargets;
        ++filesCompiled;
    }
    if (touched.lib2Linked)
    {
        ++linkTargets;
    }
    if (touched.lib2DotCpp)
    {
        ++cppTargets;
        ++filesCompiled;
    }
    if (touched.lib3Linked)
    {
        ++linkTargets;
    }
    if (touched.lib3DotCpp)
    {
        ++cppTargets;
        ++filesCompiled;
    }
    if (touched.lib4Linked)
    {
        ++linkTargets;
    }
    if (touched.lib4DotCpp)
    {
        ++cppTargets;
        ++filesCompiled;
    }

    return snapshotBalances(0, filesCompiled, cppTargets, linkTargets, linkTargetsDebug);
}