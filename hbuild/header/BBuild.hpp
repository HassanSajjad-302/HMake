

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
#include "variant"

using std::string, std::vector, std::filesystem::path, std::map, std::set, std::shared_ptr, std::tuple, std::thread,
    std::stack, std::mutex, std::enable_shared_from_this, fmt::formatter, std::atomic, std::variant;
using Json = nlohmann::ordered_json;

struct BIDD
{
    string path;
    bool copy;
    BIDD() = default;
    BIDD(const string &path_, bool copy_);
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
    NEEDS_RECOMPILE
};

struct SourceNode
{
    Node *node = nullptr;
    set<Node *> headerDependencies;
    FileStatus compilationStatus = FileStatus::NOT_ASSIGNED;
    mutable bool targetCacheSearched = false;
    mutable bool presentInCache;
    mutable bool presentInSource;
    bool headerUnit = false;
    bool materialize; // Used For Header-Units
};
void to_json(Json &j, const SourceNode &sourceNode);
void from_json(const Json &j, SourceNode &sourceNode);
bool operator==(const SourceNode &lhs, const SourceNode &rhs);
bool operator<(const SourceNode &lhs, const SourceNode &rhs);

// Used to cache the dependencies on the disk for faster compilation next time
struct BTargetCache
{
    string compileCommand;
    string linkCommand;
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
    unsigned short compilationNotStartedSize;
    unsigned short compilationNotCompletedSize;
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
    void executePrintRoutine(uint32_t color, bool printOnlyOnError) const;
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

struct LogicalNameComparator
{
    bool operator()(const struct SMRuleRequires &lhs, const struct SMRuleRequires &rhs) const;
};

struct HeaderUnitComparator
{
    bool operator()(const struct SMRuleRequires &lhs, const struct SMRuleRequires &rhs) const;
};

struct SMRuleRequires
{
    SM_REQUIRE_TYPE type = SM_REQUIRE_TYPE::NOT_ASSIGNED;
    string logicalName;
    bool angle;
    string sourcePath;
    inline static set<SMRuleRequires, HeaderUnitComparator> *setHeaderUnits;
    inline static set<SMRuleRequires, LogicalNameComparator> *setLogicalName;
    void populateFromJson(const Json &j, const string &smFilePath, const string &provideLogicalName);
};

struct SMFile // Scanned Module Rule
{
    SM_FILE_TYPE type = SM_FILE_TYPE::NOT_ASSIGNED;
    SourceNode &sourceNode;
    string logicalName;
    bool angle;
    vector<SMRuleRequires> requireDependencies;

    SMFile(SourceNode &sourceNode_, ParsedTarget &parsedTarget_);
    SMFile(SourceNode &sourceNode_, ParsedTarget &parsedTarget_, bool angle_);
    // Only used in-case of Header-Unit
    bool materialize;

    // State Variables
    string fileName;
    vector<SMFile *> fileDependencies;
    vector<SMFile *> fileDependents;
    set<SMFile *> commandLineFileDependencies;
    unsigned short needsCompileDependenciesSize = 0;

    ParsedTarget &parsedTarget;
    bool hasProvide = false;
    // unsigned int numberOfNeedsCompileDependencies;
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
    string linkOrArchiveCommand;
    string linkOrArchiveCommandPrintFirstHalf;

    bool isModule = false;
    bool libsParsed = false;
    BuildNode buildNode;
    inline static set<string> variantFilePaths;
    unsigned int smFilesToBeCompiledSize = 0;
    vector<SMFile *> smFiles;

    explicit ParsedTarget(const string &targetFilePath_);
    void executePreBuildCommands() const;
    void executePostBuildCommands() const;
    void setCompileCommand();
    inline string &getCompileCommand();
    void setSourceCompileCommandPrintFirstHalf();
    inline string &getSourceCompileCommandPrintFirstHalf();
    void setLinkOrArchiveCommandAndPrint();
    string &getLinkOrArchiveCommand();
    string &getLinkOrArchiveCommandPrintFirstHalf();
    // void parseSourceDirectoriesAndFinalizeSourceFiles();
    //  Create a new SourceNode and insert it into the targetCache.sourceFileDependencies if it is not already there. So
    //  that it is written to the cache on disk for faster compilation next time
    void setSourceNodeCompilationStatus(SourceNode &sourceNode, bool angle) const;
    // This must be called to confirm that all Prebuilt Libraries Exists. Also checks if any of them is recently
    // touched.
    void checkForRelinkPrebuiltDependencies();
    void populateSourceTree();
    void populateModuleTree(Builder &builder, map<string *, map<string, SourceNode *>> &requirePaths,
                            set<TarjanNode<SMFile>> &tarjanNodesSMFiles);
    string getInfrastructureFlags();
    string getCompileCommandPrintSecondPart(const SourceNode &sourceNode);
    PostCompile CompileSMFile(SMFile *smFile);
    PostCompile Compile(SourceNode &sourceNode);
    void populateCommandAndPrintCommandWithObjectFiles(string &command, string &printCommand,
                                                       const PathPrint &objectFilesPathPrint);
    PostBasic Archive();
    PostBasic Link();
    PostBasic GenerateSMRulesFile(const SourceNode &sourceNode, bool printOnlyOnError);

    TargetType getTargetType() const;
    void pruneAndSaveBuildCache(bool successful);
    void copyParsedTarget() const;
};
bool operator<(const ParsedTarget &lhs, const ParsedTarget &rhs);

using VariantParsedTargetString = variant<ParsedTarget, string>;
struct VariantParsedTargetStringComparator
{
    bool operator()(const VariantParsedTargetString &lhs, const VariantParsedTargetString &rhs) const;
};
class Builder
{
    set<VariantParsedTargetString, VariantParsedTargetStringComparator> parsedTargetSet;

    unsigned short linkThreadsCount = 0;
    vector<ParsedTarget *> sourceTargets;
    unsigned short sourceTargetsIndex;

    unsigned long canBeCompiledModuleIndex = 0;
    unsigned int finalSMFilesSize;

    unsigned long canBeLinkedIndex = 0;
    unsigned int totalTargetsNeedingLinking;

  public:
    // inline static int modulesActionsNeeded = 0;
    map<string *, set<SMFile, SMFileCompareWithoutHeaderUnits>> smFilesWithoutHeaderUnits;
    map<string *, set<SMFile, SMFileCompareWithHeaderUnits>> smFilesWithHeaderUnits;
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

string getReducedPath(const string &subjectPath, const PathPrint &pathPrint);
#endif // HMAKE_HBUILD_SRC_BBUILD_HPP
