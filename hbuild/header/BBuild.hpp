

#ifndef HMAKE_HBUILD_SRC_BBUILD_HPP
#define HMAKE_HBUILD_SRC_BBUILD_HPP

#include "Configure.hpp"
#include "atomic"
#include "filesystem"
#include "map"
#include "memory"
#include "mutex"
#include "nlohmann/json.hpp"
#include "stack"
#include "string"
#include "thread"
#include "tuple"

using std::string, std::vector, std::filesystem::path, std::map, std::set, std::shared_ptr, std::tuple, std::thread,
    std::stack, std::mutex, std::enable_shared_from_this, fmt::formatter, std::atomic;
using Json = nlohmann::ordered_json;

struct BIDD
{
    string path;
    bool copy;
};
void from_json(const Json &j, BIDD &bCompileDefinition);

struct BLibraryDependency
{
    string path;
    string hmakeFilePath;
    bool copy;
    bool preBuilt;
    bool imported;
};
void from_json(const Json &j, BLibraryDependency &bCompileDefinition);

struct BCompileDefinition
{
    string name;
    string value;
};
void from_json(const Json &j, BCompileDefinition &bCompileDefinition);

class Node;
template <> struct std::less<Node *>
{
    bool operator()(const Node *lhs, const Node *rhs) const;
};

class Node
{
  public:
    // This keeps info if a file is touched. If it's touched, it's not touched again.
    inline static set<Node *> allFiles{};
    string filePath;
    // Because checking for lastUpdateTime is expensive, we do it only once even if file is used in multiple targets.
    bool isUpdated = false;
    std::filesystem::file_time_type lastUpdateTime;
    void checkIfNotUpdatedAndUpdate();
    // Create a node and inserts it into the allFiles if it is not already there
    static Node *getNodeFromString(const string &str);
    string fileName;
    inline string &getFileName();
};
void to_json(Json &j, const Node *node);
void from_json(const Json &j, Node *node); // Was to be used in from_json of SourceNode but is not used because
// this does not return the modified pointer for some reason but instead returns the unmodified pointer.

enum class FileStatus
{
    NOT_ASSIGNED,
    UPDATED,
    NEEDS_RECOMPILE,
    COMPILING,
    DOES_NOT_EXIST,
};

struct SourceNode
{
    Node *node = nullptr;
    set<Node *> headerDependencies;
    FileStatus compilationStatus = FileStatus::NOT_ASSIGNED;
    mutable bool targetCacheSearched = false;
    mutable bool presentInCache;
    mutable bool presentInSource;
};
void to_json(Json &j, const SourceNode &sourceNode);
void from_json(const Json &j, SourceNode &sourceNode);

bool operator<(const SourceNode &lhs, const SourceNode &rhs);

// Used to cache the dependencies on the disk for faster compilation next time
struct BTargetCache
{
    string compileCommand;
    set<SourceNode> sourceFileDependencies;
    SourceNode &addNodeInSourceFileDependencies(const string &str);
};
void to_json(Json &j, const BTargetCache &bTargetCache);
void from_json(const Json &j, BTargetCache &bTargetCache);

// TODO: I think I should add all of the variables of this struct in ParsedTarget and then totally remove this. This
// will make ParsedTarget class bigger but I have never used BuildNode alone e.g. set<BuildNode> or vector<BuildNode> is
// never used.
struct BuildNode
{
    BTargetCache targetCache;
    bool needsLinking = false;
    vector<SourceNode *> outdatedFiles;
    unsigned short outdatedFilesSizeIndex = 0;
    unsigned short filesUpdated = 0;
    vector<struct ParsedTarget *> linkDependents;
    unsigned short needsLinkDependenciesSize = 0;
};

struct PostBasic
{
    string printCommand;
    string commandSuccessOutput;
    string commandErrorOutput;
    bool successfullyCompleted;

    /* Could be a target or a file. For target (link and archive), we add extra _t at the end of the target name.*/
    bool isTarget;

    // command is 3 parts. 1) tool path 2) command without output and error files 3) output and error files.
    // while print is 2 parts. 1) tool path and command without output and error files. 2) output and error files.
    explicit PostBasic(const BuildTool &buildTool, const string &commandFirstHalf, string printCommandFirstHalf,
                       const string &buildCacheFilesDirPath, const string &fileName, const PathPrint &pathPrint,
                       bool isTarget_);
    void executePrintRoutine(uint32_t color) const;
};

struct ParsedTarget;
struct PostCompile : PostBasic
{
    ParsedTarget &parsedTarget;

    explicit PostCompile(const ParsedTarget &parsedTarget_, const BuildTool &buildTool, const string &commandFirstHalf,
                         string printCommandFirstHalf, const string &buildCacheFilesDirPath, const string &fileName,
                         const PathPrint &pathPrint);
    bool checkIfFileIsInEnvironmentIncludes(const string &str);
    void parseDepsFromMSVCTextOutput(SourceNode &sourceNode);
    void parseDepsFromGCCDepsOutput(SourceNode &sourceNode);
    void executePostCompileRoutineWithoutMutex(SourceNode &sourceNode);
};

enum class SM_REQUIRE_TYPE : unsigned short
{
    NOT_ASSIGNED = 0,
    PRIMARY_EXPORT = 1,
    PARTITION_EXPORT = 2,
    HEADER_UNIT = 3,
};

enum class SM_FILE_TYPE : unsigned short
{
    NOT_ASSIGNED = 0,
    PRIMARY_EXPORT = 1,
    PARTITION_EXPORT = 2,
    HEADER_UNIT = 3,
    PRIMARY_IMPLEMENTATION = 4,
    PARTITION_IMPLEMENTATION = 5,
};

bool operator<(const struct SMRuleRequires &lhs, const struct SMRuleRequires &rhs);
struct SMRuleRequires
{
    SM_REQUIRE_TYPE type = SM_REQUIRE_TYPE::NOT_ASSIGNED;
    string logicalName;
    bool angle;
    string sourcePath;
    inline static set<SMRuleRequires> *smRuleRequiresSet;
    void populateFromJson(const Json &j, const string &smFilePath);
};
template <> struct formatter<SM_REQUIRE_TYPE> : formatter<std::string>
{
    auto format(SM_REQUIRE_TYPE smRequireType, format_context &ctx)
    {
        string s;
        if (smRequireType == SM_REQUIRE_TYPE::NOT_ASSIGNED)
        {
            s = "NOT_ASSIGNED";
        }
        else if (smRequireType == SM_REQUIRE_TYPE::PRIMARY_EXPORT)
        {
            s = "PRIMARY_EXPORT";
        }
        else if (smRequireType == SM_REQUIRE_TYPE::PARTITION_EXPORT)
        {
            s = "PARTITION_EXPORT";
        }
        else
        {
            s = "HEADER_UNIT";
        }
        return formatter<string>::format(fmt::format("{}", s), ctx);
    }
};
template <> struct formatter<SMRuleRequires> : formatter<std::string>
{
    auto format(SMRuleRequires smRuleRequires, format_context &ctx)
    {
        if (smRuleRequires.type == SM_REQUIRE_TYPE::HEADER_UNIT)
        {
            return formatter<string>::format(
                fmt::format("LogicalName:\t{}\nType:\t{}\nIncludeAngle\t{}\nSourcePath\t{}\n",
                            smRuleRequires.logicalName, smRuleRequires.type, smRuleRequires.angle,
                            smRuleRequires.sourcePath),
                ctx);
        }
        else
        {
            return formatter<string>::format(
                fmt::format("LogicalName:\t{}\nType:\t{}\n", smRuleRequires.logicalName, smRuleRequires.type), ctx);
        }
    }
};

struct SMFile // Scanned Module Rule
{
    SM_FILE_TYPE type = SM_FILE_TYPE::NOT_ASSIGNED;
    SourceNode &sourceNode;
    string logicalName;
    bool angle = false;
    vector<SMRuleRequires> requireDependencies;

    SMFile(SourceNode &sourceNode_, ParsedTarget &parsedTarget_);
    SMFile(SourceNode &sourceNode_, ParsedTarget &parsedTarget_, bool angle_);
    // Only used in-case of Header-Unit
    bool materialize = false;

    // State Variables
    string fileName;
    vector<SMFile *> fileDependencies;
    vector<SMFile *> fileDependents;
    set<SMFile *> commandLineFileDependencies;
    ParsedTarget &parsedTarget;
    bool hasProvide = false;
    unsigned int numberOfDependencies;
    void populateFromJson(const Json &j);
    string getFlag(const string &outputFilesWithoutExtension) const;
    string getFlagPrint(const string &outputFilesWithoutExtension) const;
    string getRequireFlag(const string &outputFilesWithoutExtension) const;
    string getRequireFlagPrint(const string &outputFilesWithoutExtension) const;
    string getModuleCompileCommandPrintLastHalf() const;
};

struct SMFileCompareWithHeaderUnits
{
    bool operator()(const SMFile &lhs, const SMFile &rhs) const;
};

struct SMFileCompareWithoutHeaderUnits
{
    bool operator()(const SMFile &lhs, const SMFile &rhs) const;
};

template <typename T> struct TarjanNode
{
    // Input
    inline static set<TarjanNode> *tarjanNodes;

    // Following 4 are reset in findSCCS();
    inline static int index = 0;
    inline static vector<TarjanNode *> stack;

    // Output
    inline static vector<T *> cycle;
    inline static bool cycleExists;
    inline static vector<T *> topologicalSort;
    // inline static map<int, set<int>> reverseDeps;

    explicit TarjanNode(const T *id_);
    const T *const id;
    mutable vector<TarjanNode *> deps;
    bool initialized = false;
    bool onStack;
    int nodeIndex;
    int lowLink;
    // Find Strongly Connected Components
    static void findSCCS();
    void strongConnect();
};
template <typename T> bool operator<(const TarjanNode<T> &lhs, const TarjanNode<T> &rhs);

template <typename T> TarjanNode<T>::TarjanNode(const T *const id_) : id{id_}
{
}

template <typename T> void TarjanNode<T>::findSCCS()
{
    index = 0;
    cycleExists = false;
    cycle.clear();
    stack.clear();
    topologicalSort.clear();
    // reverseDeps.clear();
    for (auto it = tarjanNodes->begin(); it != tarjanNodes->end(); ++it)
    {
        auto &b = *it;
        auto &tarjanNode = const_cast<TarjanNode<T> &>(b);
        if (!tarjanNode.initialized)
        {
            tarjanNode.strongConnect();
        }
    }
}

template <typename T> void TarjanNode<T>::strongConnect()
{
    initialized = true;
    nodeIndex = TarjanNode<T>::index;
    lowLink = TarjanNode<T>::index;
    ++TarjanNode<T>::index;
    stack.push_back(this);
    onStack = true;

    for (TarjanNode<T> *tarjandep : deps)
    {
        if (!tarjandep->initialized)
        {
            tarjandep->strongConnect();
            lowLink = std::min(lowLink, tarjandep->lowLink);
        }
        else if (tarjandep->onStack)
        {
            lowLink = std::min(lowLink, tarjandep->nodeIndex);
        }
    }

    vector<TarjanNode<T> *> tempCycle;
    if (lowLink == nodeIndex)
    {
        while (true)
        {
            TarjanNode<T> *tarjanTemp = stack.back();
            stack.pop_back();
            tarjanTemp->onStack = false;
            tempCycle.emplace_back(tarjanTemp);
            if (tarjanTemp->id == this->id)
            {
                break;
            }
        }
        if (cycle.size() > 1)
        {
            for (TarjanNode<T> *c : tempCycle)
            {
                cycle.emplace_back(const_cast<T *>(c->id));
            }
            cycleExists = true;
            return;
            // Todo
            //  Check if this is forwards or backwards
        }
    }
    topologicalSort.emplace_back(const_cast<T *>(id));
}

template <typename T> bool operator<(const TarjanNode<T> &lhs, const TarjanNode<T> &rhs)
{
    return lhs.id < rhs.id;
}

class Builder;
struct ParsedTarget : public enable_shared_from_this<ParsedTarget>
{
    friend struct PostCompile;
    // Parsed Info Not Changed Once Read
    string targetFilePath;
    string targetName;
    const string *variantFilePath;
    Compiler compiler;
    Linker linker;
    Archiver archiver;
    Environment environment;
    string compilerFlags;
    string linkerFlags;
    set<string> sourceFiles;
    set<string> modulesSourceFiles;
    vector<BLibraryDependency> libraryDependencies;
    vector<BIDD> includeDirectories;
    vector<string> libraryDirectories;
    string compilerTransitiveFlags;
    string linkerTransitiveFlags;
    vector<BCompileDefinition> compileDefinitions;
    vector<string> preBuildCustomCommands;
    vector<string> postBuildCustomCommands;
    string buildCacheFilesDirPath;
    TargetType targetType;
    Json consumerDependenciesJson;
    string packageTargetPath;
    bool packageMode;
    bool copyPackage;
    string packageName;
    string packageCopyPath;
    int packageVariantIndex;
    string outputName;
    string outputDirectory;

    // Some State Variables
    set<ParsedTarget *> libraryParsedTargetDependencies;
    string actualOutputName;
    string compileCommand;
    string sourceCompileCommandPrintFirstHalf;

    bool isModule = false;
    bool libsParsed = false;
    BuildNode buildNode;
    inline static set<string> variantFilePaths;
    unsigned int smFilesToBeCompiledSize = 0;
    vector<SMFile *> smFiles;

    explicit ParsedTarget(const string &targetFilePath_);
    void parseTarget();
    // void setActualOutputName();
    void executePreBuildCommands() const;
    void executePostBuildCommands() const;
    void setCompileCommand();
    inline string &getCompileCommand();
    void setSourceCompileCommandPrintFirstHalf();
    inline string &getSourceCompileCommandPrintFirstHalf();

    // void parseSourceDirectoriesAndFinalizeSourceFiles();
    //  Create a new SourceNode and insert it into the targetCache.sourceFileDependencies if it is not already there. So
    //  that it is written to the cache on disk for faster compilation next time
    void setSourceNodeCompilationStatus(SourceNode &sourceNode, bool checkSMFileExists,
                                        const string &extension = "") const;
    // This must be called to confirm that all Prebuilt Libraries Exists. Also checks if any of them is recently
    // touched.
    void checkForRelinkPrebuiltDependencies();
    void populateSourceTree();
    void populateModuleTree(Builder &builder, map<string, SourceNode *> &requirePaths,
                            set<TarjanNode<SMFile>> &tarjanNodesSMFiles);
    string getInfrastructureFlags();
    string getCompileCommandPrintSecondPart(const SourceNode &sourceNode);
    PostCompile CompileSMFile(SMFile *smFile);
    PostCompile Compile(SourceNode &sourceNode);
    void populateCommandAndPrintCommandWithObjectFiles(string &command, string &printCommand,
                                                       const PathPrint &objectFilesPathPrint);
    PostBasic Archive();
    PostBasic Link();
    PostBasic GenerateSMRulesFile(const SourceNode &sourceNode);

    TargetType getTargetType() const;
    void pruneAndSaveBuildCache() const;
    void copyParsedTarget() const;
};
bool operator<(const ParsedTarget &lhs, const ParsedTarget &rhs);

class Builder
{
    set<ParsedTarget> parsedTargetSet;

    unsigned short buildThreadsAllowed = 12;
    unsigned short maximumLinkThreadsAllowed = 1;
    unsigned short currentLinkingThreads = 0;

    vector<ParsedTarget *> sourceTargets;
    unsigned short sourceTargetsIndex;

    unsigned long canBeCompiledModuleIndex = 0;
    unsigned int finalSMFilesSize;

    unsigned long canBeLinkedIndex = 0;
    unsigned int finalCanBeLinkedSize;

  public:
    // inline static int modulesActionsNeeded = 0;
    set<SMFile, SMFileCompareWithoutHeaderUnits> smFilesWithoutHeaderUnits;
    set<SMFile, SMFileCompareWithHeaderUnits> smFilesWithHeaderUnits;
    vector<SMFile *> canBeCompiledModule;
    vector<ParsedTarget *> canBeLinked;

    Builder(const set<string> &variantFilePath);
    void populateParsedTargetSetAndModuleTargets(const set<string> &targetFilePaths,
                                                 vector<ParsedTarget *> &moduleTargets);
    int populateCanBeCompiledAndReturnModuleThreadsNeeded(const vector<ParsedTarget *> &moduleTargets);
    void removeRedundantNodesFromSourceTree();
    static set<string> getTargetFilePathsFromVariantFile(const string &fileName);
    static set<string> getTargetFilePathsFromProjectOrPackageFile(const string &fileName, bool isPackage);
    // This function is executed by multiple threads and is executed recursively until build is finished.
    void buildSourceFiles();
    void buildModuleFiles();
    void linkTargets();

    static void copyPackage(const path &packageFilePath);
};

inline Settings settings;

string getReducedPath(const string &subjectPath, const PathPrint &pathPrint);
#endif // HMAKE_HBUILD_SRC_BBUILD_HPP
