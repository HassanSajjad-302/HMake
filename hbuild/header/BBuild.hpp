

#ifndef HMAKE_HBUILD_SRC_BBUILD_HPP
#define HMAKE_HBUILD_SRC_BBUILD_HPP

#include "Configure.hpp"
#include "filesystem"
#include "map"
#include "memory"
#include "nlohmann/json.hpp"
#include "string"

using std::string, std::vector, std::filesystem::path, std::map, std::set, std::shared_ptr;
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
    bool isUpdated;
    std::filesystem::file_time_type lastUpdateTime;
    bool operator<(const Node &node) const;
};

struct SourceNode
{
    Node *node;
    bool fileExists;
    bool needsRecompile;
    bool operator<(const SourceNode &sourceNode) const;
};
struct BTargetCache
{
    string compileCommand;
    // This maps source files with their dependencies.
    map<SourceNode *, set<Node *>> sourceFileDependencies;
};
void to_json(Json &j, const BTargetCache &bTargetCache);
void from_json(const Json &j, BTargetCache &bTargetCache);

class BTarget
{
    // Parsed info
    string targetName;
    Compiler compiler;
    Linker linker;
    Archiver staticLibraryTool;
    vector<string> environmentIncludeDirectories;
    vector<string> environmentLibraryDirectories;
    string environmentCompilerFlags;
    string environmentLinkerFlags;
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
    bool havePreBuildCommandsExecuted = false;
    string actualOutputName;
    string compileCommand;
    std::filesystem::file_time_type lastOutputTouchTime;

    // This keeps info if a file is touched. If it's touched, it's not touched again.
    static set<Node *> allFiles;
    // This info is used in conjunction with source files to decide which files need a recompile.
    BTargetCache targetCache;

  public:
    explicit BTarget(const string &targetFilePath);
    void setActualOutputName();
    void executePreBuildCommands();
    bool checkIfAlreadyBuiltAndCreatNecessaryDirectories();
    void setCompileCommand();
    void parseSourceDirectoriesAndFinalizeSourceFiles();
    SourceNode *getSourceNode(SourceNode *sourceNode);
    Node *getNode(Node *node);
    void build();
    int getNumberOfUpdates();
};

// BuildPreBuiltTarget
struct BPTarget
{
    explicit BPTarget(const string &targetFilePath, const path &copyFrom);
};

struct BVariant
{
    explicit BVariant(const path &variantFilePath);
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
