

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
    string fileName;
    std::filesystem::file_time_type lastUpdateTime;
    explicit Node(string filePath_);
    // Because checking for lastUpdateTime is expensive, we do it only once even if file is used in multiple targets.
    bool isUpdated = false;
    void checkIfNotUpdatedAndUpdate();
    // Create a node and inserts it into the allFiles if it is not already there
    static Node *getNodeFromString(const string &str);
};

void to_json(Json &j, const Node *node);

void from_json(const Json &j, Node *node); // Was to be used in from_json of SourceNode but is not used because
// this does not return the modified pointer for some reason but instead returns the unmodified pointer.

enum class FileStatus
{
    NOT_ASSIGNED,
    UPDATED,
    NEEDS_UPDATE
};

struct ReportResultTo
{
};

enum class ResultType
{
    LINK,
    SOURCENODE,
    CPP_SMFILE,
    CPP_MODULE,
    ACTIONTARGET,
};

TarjanNode(const struct BTarget *) -> TarjanNode<BTarget>;
using TBT = TarjanNode<BTarget>;
inline set<TBT> tarjanNodesBTargets;

struct IndexInTopologicalSortComparator
{
    bool operator()(const BTarget *lhs, const BTarget *rhs) const;
};

struct BTarget
{
    vector<BTarget *> sameTypeDependents;
    set<BTarget *> dependents;
    set<BTarget *> dependencies;
    // This includes dependencies and their dependencies arranged on basis of indexInTopologicalSort.
    set<BTarget *, IndexInTopologicalSortComparator> allDependencies;
    inline static unsigned long total = 0;
    unsigned long id = 0; // unique for every BTarget
    unsigned long topologicallySortedId = 0;
    ReportResultTo *const reportResultTo;
    unsigned int dependenciesSize = 0;
    ResultType resultType;
    FileStatus fileStatus = FileStatus::UPDATED;
    // This points to the tarjanNodeBTargets vector element in Builder where set could had been used. But, this is for
    // optimization, as except SMFile Generation, all BTargets are topologically sorted.
    TBT *tarjanNode = nullptr;
    // Value is assigned on basis of TBT::topologicalSort index. Targets in allDependencies vector are arranged by this
    // value.
    unsigned long indexInTopologicalSort = 0;
    explicit BTarget(ReportResultTo *reportResultTo_, ResultType resultType_);
    void addDependency(BTarget &dependency);
};

bool operator<(const BTarget &lhs, const BTarget &rhs);

struct CachedFile
{
    Node *node = nullptr;
    bool presentInCache = false;
    bool presentInSource = false;
    explicit CachedFile(const string &filePath);
};

struct SourceNode : public CachedFile, public BTarget
{
    set<Node *> headerDependencies;
    SourceNode(const string &filePath, ReportResultTo *reportResultTo_, ResultType resultType_);
};

void to_json(Json &j, const SourceNode &sourceNode);
bool operator<(const SourceNode &lhs, const SourceNode &rhs);

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
    GLOBAL_MODULE_FRAGMENT = 6,
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

    bool populateFromJson(const Json &j, const string &smFilePath, const string &provideLogicalName);
};

struct HeaderUnitConsumer
{
    bool angle;
    string logicalName;
    HeaderUnitConsumer(bool angle_, const string &logicalName_);
};
bool operator<(const HeaderUnitConsumer &lhs, const HeaderUnitConsumer &rhs);

struct SMFile : public SourceNode // Scanned Module Rule
{
    SM_FILE_TYPE type = SM_FILE_TYPE::NOT_ASSIGNED;
    string logicalName;
    bool angle;
    vector<SMRuleRequires> requireDependencies;

    SMFile(const string &srcPath, ParsedTarget *parsedTarget_);

    // State Variables
    map<SMFile *, set<HeaderUnitConsumer>> headerUnitsConsumptionMethods;
    vector<SMFile *> fileDependencies;
    set<SMFile *> commandLineFileDependencies;

    ParsedTarget &parsedTarget;
    bool hasProvide = false;
    bool standardHeaderUnit = false;

    void populateFromJson(const Json &j, struct Builder &builder);
    string getFlag(const string &outputFilesWithoutExtension) const;
    string getFlagPrint(const string &outputFilesWithoutExtension) const;
    string getRequireFlag(const SMFile &dependentSMFile) const;
    string getRequireFlagPrint(const SMFile &logicalName_) const;
    string getModuleCompileCommandPrintLastHalf() const;
};

struct SMFileVariantPathAndLogicalName
{
    const string &logicalName;
    const string &variantFilePath;
    SMFileVariantPathAndLogicalName(const string &logicalName_, const string &variantFilePath_);
};

struct SMFilePointerComparator
{
    bool operator()(const SMFile *lhs, const SMFile *rhs) const;
};

struct SMFilePathAndVariantPathComparator
{
    bool operator()(const SMFile &lhs, const SMFile &rhs) const;
};

struct SMFileCompareLogicalName
{
    bool operator()(SMFileVariantPathAndLogicalName lhs, SMFileVariantPathAndLogicalName rhs) const;
};

class Builder;

struct ParsedTarget : public ReportResultTo, public BTarget
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
    string actualOutputName;
    // Compile Command excluding source-file or source-files(in case of module) that is also stored in the cache.
    string compileCommand;
    string sourceCompileCommandPrintFirstHalf;
    // Link Command excluding libraries(pre-built or other) that is also stored in the cache.
    string linkOrArchiveCommand;
    string linkOrArchiveCommandPrintFirstHalf;

    // bool isModule = false;
    bool libsParsed = false;
    // This will hold set of sourceNodes or smFiles depending on the isModule bool
    // Used to cache the dependencies on the disk for faster compilation next time

    // TargetCache related Variables
    //  Decides if the set<SourceNode> will actually hold the SMFile
    string linkCommand;
    set<SourceNode> sourceFileDependencies;
    // Comparator used is same as for SourceNode
    set<SMFile> moduleSourceFileDependencies;
    SourceNode &addNodeInSourceFileDependencies(const string &str);
    SMFile &addNodeInModuleSourceFileDependencies(const std::string &str, bool angle);

    inline static set<string> variantFilePaths;
    vector<SMFile *> smFiles;

    explicit ParsedTarget(const string &targetFilePath_);

    void executePreBuildCommands() const;
    void executePostBuildCommands() const;
    string &getCompileCommand();
    void setCompileCommand();
    void setSourceCompileCommandPrintFirstHalf();
    inline string &getSourceCompileCommandPrintFirstHalf();
    void setLinkOrArchiveCommandAndPrint();
    string &getLinkOrArchiveCommand();
    string &getLinkOrArchiveCommandPrintFirstHalf();

    //  Create a new SourceNode and insert it into the targetCache.sourceFileDependencies if it is not already there. So
    //  that it is written to the cache on disk for faster compilation next time
    void setSourceNodeFileStatus(SourceNode &sourceNode, bool angle) const;

    // This must be called to confirm that all Prebuilt Libraries Exists. Also checks if any of them is recently
    // touched.
    void checkForRelinkPrebuiltDependencies();
    void checkForPreBuiltAndCacheDir();
    void populateSetTarjanNodesSourceNodes(Builder &builder);
    void populateRequirePathsAndSMFilesWithHeaderUnitsMap(
        Builder &builder, map<SMFileVariantPathAndLogicalName, SMFile *, SMFileCompareLogicalName> &requirePaths);
    void populateParsedTargetModuleSourceFileDependenciesAndBuilderSMFiles(Builder &builder);
    string getInfrastructureFlags();
    string getCompileCommandPrintSecondPart(const SourceNode &sourceNode);
    PostCompile CompileSMFile(SMFile *smFile);
    PostCompile Compile(SourceNode &sourceNode);
    /* void populateCommandAndPrintCommandWithObjectFiles(string &command, string &printCommand,
                                                        const PathPrint &objectFilesPathPrint);*/
    PostBasic Archive();
    PostBasic Link();
    PostBasic GenerateSMRulesFile(const SourceNode &sourceNode, bool printOnlyOnError);
    TargetType getTargetType() const;
    void pruneAndSaveBuildCache(bool successful);
    void copyParsedTarget() const;
    void execute(unsigned long fileTargetId);
    void executePrintRoutine(unsigned long fileTargetId);
};

bool operator<(const ParsedTarget &lhs, const ParsedTarget &rhs);

struct SystemHeaderUnit
{
    ParsedTarget *parsedTarget;
    string filePath;
    SystemHeaderUnit(ParsedTarget &parsedTarget_, const string &filePath_);
};
bool operator<(const SystemHeaderUnit &lhs, const SystemHeaderUnit &rhs);

struct ApplicationHeaderUnit : CachedFile
{
    ParsedTarget *parsedTarget;
    ApplicationHeaderUnit(ParsedTarget &parsedTarget_, const string &cachedFilePath_);
};
bool operator<(const ApplicationHeaderUnit &lhs, const ApplicationHeaderUnit &rhs);

struct HeaderUnits
{
    set<SystemHeaderUnit> systemHeaderUnits;
    set<ApplicationHeaderUnit> applicationHeaderUnits;
};

class Builder
{
    set<ParsedTarget> parsedTargetSet;
    vector<ParsedTarget *> sourceTargets;
    unsigned long finalBTargetsIndex = 0;
    unsigned int finalBTargetsSizeGoal;

  public:
    vector<BTarget *> filteredBTargets;
    vector<BTarget *> finalBTargets;

    set<SMFile *, SMFilePointerComparator> smFiles;
    set<SMFile, SMFilePathAndVariantPathComparator> headerUnits;
    vector<SMFile *> canBeCompiledModule;
    vector<ParsedTarget *> canBeLinked;
    explicit Builder(const set<string> &variantFilePath);
    void populateParsedTargetsSetAndSetTarjanNodesParsedTargets(const set<string> &targetFilePaths);
    void checkForHeaderUnitsCache();
    void populateSetTarjanNodesBTargetsSMFiles(
        const map<SMFileVariantPathAndLogicalName, SMFile *, SMFileCompareLogicalName> &requirePaths);
    void populateFinalBTargets();
    static set<string> getTargetFilePathsFromVariantFile(const string &fileName);
    static set<string> getTargetFilePathsFromProjectOrPackageFile(const string &fileName, bool isPackage);

    // This function is executed by multiple threads and is executed recursively until build is finished.
    void launchThreadsAndUpdateBTargets();
    void updateBTargets();
    static void copyPackage(const path &packageFilePath);
    void createHeaderUnits();
};

string getReducedPath(const string &subjectPath, const PathPrint &pathPrint);

#endif // HMAKE_HBUILD_SRC_BBUILD_HPP
