

#ifndef HMAKE_HBUILD_SRC_BBUILD_HPP
#define HMAKE_HBUILD_SRC_BBUILD_HPP

#include "Configure.hpp"
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
    std::stack, std::mutex;
using Json = nlohmann::ordered_json;

enum BTargetType
{
    EXECUTABLE,
    STATIC,
    SHARED,
    PLIBRARY_STATIC,
    PLIBRARY_SHARED
};
void from_json(const Json &j, BTargetType &targetType);

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

class Node
{
  public:
    string filePath;
    // Because checking for lastUpdateTime is expensive, we do it only once even if file is used in multiple targets.
    bool isUpdated = false;
    std::filesystem::file_time_type lastUpdateTime;
    void checkIfNotUpdatedAndUpdate();
};

template <> struct std::less<Node *>
{
    bool operator()(const Node *lhs, const Node *rhs) const;
};

struct SourceNode
{
    Node *node;
    bool fileExists = false;
    bool needsRecompile = false;
};

template <> struct std::less<SourceNode>
{
    bool operator()(const SourceNode &lhs, const SourceNode &rhs) const;
};

// Used to cache the dependencies on the disk for faster compilation next time
struct BTargetCache
{
    string compileCommand;
    // This maps source files with their header dependencies.
    map<SourceNode *, set<Node *>> sourceFileDependencies;
};
void to_json(Json &j, const BTargetCache &bTargetCache);
void from_json(const Json &j, BTargetCache &bTargetCache);

struct BuildNode
{
    class BTarget *target;
    stack<SourceNode *> nodes;
    size_t filesCompiled = 0;
    size_t sourceNodesStackSize = 0;
    bool startedLinking = false;
};

class BTarget
{
    // Parsed info
    string targetName;
    Compiler compiler;
    Linker linker;
    Archiver archiver;
    Environment environment;
    string compilerFlags;
    string linkerFlags;
    vector<string> sourceFiles;
    vector<SourceDirectory> sourceDirectories;
    vector<BLibraryDependency> libraryDependencies;
    vector<BIDD> includeDirectories;
    vector<string> libraryDirectories;
    string compilerTransitiveFlags;
    string linkerTransitiveFlags;
    vector<BCompileDefinition> compileDefinitions;
    vector<string> preBuildCustomCommands;
    vector<string> postBuildCustomCommands;
    string buildCacheFilesDirPath;
    BTargetType targetType;
    string targetFileName;
    Json consumerDependenciesJson;
    path packageTargetPath;
    bool packageMode;
    bool copyPackage;
    string packageName;
    string packageCopyPath;
    int packageVariantIndex;
    string outputName;
    string outputDirectory;

    // Others
    vector<BTarget> libraryDependenciesBTargets;
    bool havePreBuildCommandsExecuted = false;
    string actualOutputName;
    string compileCommand;
    std::filesystem::file_time_type lastOutputTouchTime;

    // This keeps info if a file is touched. If it's touched, it's not touched again.
    inline static set<Node *> allFiles{};
    // This info is used in conjunction with source files to decide which files need a recompile.
    BTargetCache targetCache;

    // Following acts like the build tree
    vector<BuildNode> buildTree;
    decltype(buildTree)::reverse_iterator compileIterator;
    decltype(compileIterator) linkIterator;
    int buildThreadsAllowed = 4;
    bool launchmoreThreads = true;
    mutex &oneAndOnlyMutex;

  public:
    explicit BTarget(const string &targetFilePath, mutex &m, vector<string> dependents = {});
    void checkForCircularDependencies(const vector<string> &dependents);
    void setActualOutputName();
    void executePreBuildCommands();
    bool checkIfAlreadyBuiltAndCreatNecessaryDirectories();
    void setCompileCommand();
    void parseSourceDirectoriesAndFinalizeSourceFiles();
    // Create a node and inserts it into the allFiles if it is not already there
    static Node *getNodeFromString(const string &str);
    // Create a new SourceNode and insert it into the targetCache.sourceFileDependencies if it is not already there. So
    // that it is written to the cache on disk for faster compilation next time
    SourceNode *getSourceNodeFromString(const string &str);
    void addAllSourceFilesInBuildNode(BuildNode &buildNode);
    void popularizeBuildTree(vector<BuildNode> &localBuildTree);
    bool checkIfFileIsInEnvironmentIncludes(const string &str);
    string parseDepsFromMSVCTextOutput(SourceNode *sourceNode, const string &output);
    void Compile(SourceNode *soureNode);
    void Link();
    // This function is executed by multiple threads and is executed recursively until build is finished.
    void actuallyBuild();
    void build();
};

// BuildPreBuiltTarget
struct BPTarget
{
    explicit BPTarget(const string &targetFilePath, const path &copyFrom);
};

struct BVariant
{
    explicit BVariant(const path &variantFilePath, mutex &m);
};

struct BProject
{
    explicit BProject(const path &projectFilePath);
};

struct BPackage
{
    explicit BPackage(const path &packageFilePath);
};
#endif // HMAKE_HBUILD_SRC_BBUILD_HPP
