

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
};
void to_json(Json &j, const Node *node);
void from_json(const Json &j, Node *node); // Was to be used in from_json of SourceNode but is not used because
// this does not return the modified pointer for some reason but instead returns the unmodified pointer.

enum class FileStatus
{
    UPDATED,
    NEEDS_RECOMPILE,
    COMPILING,
    DOES_NOT_EXIST,
};

struct SourceNode
{
    Node *node = nullptr;
    set<Node *> headerDependencies;
    FileStatus compilationStatus;
};
void to_json(Json &j, const SourceNode &sourceNode);
void from_json(const Json &j, SourceNode &sourceNode);

template <> struct std::less<SourceNode>
{
    bool operator()(const SourceNode &lhs, const SourceNode &rhs) const;
};

// Used to cache the dependencies on the disk for faster compilation next time
struct BTargetCache
{
    string compileCommand;
    set<SourceNode> sourceFileDependencies;
    SourceNode &addNodeInSourceFileDependencies(const string &str);
    bool areFilesLeftToCompile() const;
    bool areAllFilesCompiled() const;
    int maximumThreadsNeeded() const;
    SourceNode &getNextNodeForCompilation();
};
void to_json(Json &j, const BTargetCache &bTargetCache);
void from_json(const Json &j, BTargetCache &bTargetCache);

struct BuildNode
{
    class ParsedTarget *target = nullptr;
    BTargetCache targetCache;
    bool isLinking = false;
};

class ParsedTarget
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

    // Some State Variables
    vector<ParsedTarget> libraryDependenciesBTargets;
    string actualOutputName;
    string compileCommand;
    std::filesystem::file_time_type lastOutputTouchTime;

  public:
    explicit ParsedTarget(const string &targetFilePath, vector<string> dependents = {});
    void checkForCircularDependencies(const vector<string> &dependents);
    void setActualOutputName();
    void executePreBuildCommands();
    bool checkIfAlreadyBuiltAndCreatNecessaryDirectories();
    void setCompileCommand();
    void parseSourceDirectoriesAndFinalizeSourceFiles();
    // Create a new SourceNode and insert it into the targetCache.sourceFileDependencies if it is not already there. So
    // that it is written to the cache on disk for faster compilation next time
    void addAllSourceFilesInBuildNode(BTargetCache &bTargetCache);
    void popularizeBuildTree(vector<BuildNode> &localBuildTree);
    bool checkIfFileIsInEnvironmentIncludes(const string &str);
    string parseDepsFromMSVCTextOutput(SourceNode &sourceNode, const string &output);
    void Compile(SourceNode &soureNode);
    void Link();
    void saveBuildCache(const BTargetCache &bTargetCache);
};

class Builder
{
    vector<BuildNode> buildTree;
    decltype(buildTree)::reverse_iterator compileIterator;
    decltype(compileIterator) linkIterator;
    int buildThreadsAllowed = 4;
    mutex &oneAndOnlyMutex;

  public:
    Builder(vector<string> &targetFilePaths, mutex &oneAndOnlyMutex);
    // This function is executed by multiple threads and is executed recursively until build is finished.
    void actuallyBuild();
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
