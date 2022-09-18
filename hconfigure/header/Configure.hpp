
#ifndef HMAKE_CONFIGURE_HPP
#define HMAKE_CONFIGURE_HPP

#include "filesystem"
#include "fmt/color.h"
#include "nlohmann/json.hpp"
#include "stack"
#include "thread"
#include "utility"
#include <set>

using std::filesystem::path, std::string, std::vector, std::tuple, std::map, std::set, std::same_as, std::stack,
    std::same_as;
using Json = nlohmann::ordered_json;
namespace fs = std::filesystem;

inline string srcDir;
inline string configureDir;

inline const string pes = "{}"; // paranthesis-escape-sequence, meant to be used in fmt::print.
std::string writePath(const path &writePath);
string addQuotes(const string &pathString);
string addEscapedQuotes(const string &pathString);

struct JConsts
{
    inline static const string addQuotes = "add-quotes";
    inline static const string archive = "archive";
    inline static const string archiveCommandColor = "archive-command-color";
    inline static const string archivePrintSettings = "archive-print-settings";
    inline static const string archiver = "archiver";
    inline static const string archiverArray = "archiver-array";
    inline static const string archiverSelectedArrayIndex = "archiver-selected-array-index";
    inline static const string binary = "binary";
    inline static const string cacheIncludes = "cache-includes";
    inline static const string cacheVariables = "cache-variables";
    inline static const string clang = "clang";
    inline static const string compileCommand = "compile-command";
    inline static const string compileCommandColor = "compile-command-color";
    inline static const string compileConfigureCommands = "compile-configure-commands";
    inline static const string compileDefinitions = "compile-definitions";
    inline static const string compilePrintSettings = "compile-print-settings";
    inline static const string compiler = "compiler";
    inline static const string compilerArray = "compiler-array";
    inline static const string compilerFlags = "compiler-flags";
    inline static const string compilerSelectedArrayIndex = "compiler-selected-array-index";
    inline static const string compilerTransitiveFlags = "compiler-transitive-flags";
    inline static const string configuration = "configuration";
    inline static const string consumerDependencies = "consumer-dependencies";
    inline static const string copy = "copy";
    inline static const string copyingPackage = "copying-package";
    inline static const string copyingTarget = "copying-target";
    inline static const string debug = "debug";
    inline static const string dependencies = "dependencies";
    inline static const string depth = "depth";
    inline static const string directories = "directories";
    inline static const string environment = "environment";
    inline static const string environmentCompilerFlags = "environment-compiler-flags";
    inline static const string environmentIncludeDirectories = "environment-include-directories";
    inline static const string environmentLibraryDirectories = "environment-library-directories";
    inline static const string executable = "executable";
    inline static const string family = "family";
    inline static const string files = "files";
    inline static const string gcc = "gcc";
    inline static const string generalPrintSettings = "general-print-settings";
    inline static const string hbuildErrorOutput = "hbuild-error-output";
    inline static const string hbuildSequenceOutput = "hbuild-sequence-output";
    inline static const string hbuildStatementOutput = "hbuild-statement-output";
    inline static const string headerDependencies = "header-dependencies";
    inline static const string hmakeFilePath = "hmake-file-path";
    inline static const string ifcOutputFile = "ifc-output-file";
    inline static const string importedFromOtherHmakePackage = "imported-from-other-hmake-package";
    inline static const string importedFromOtherHmakePackageFromOtherHmakePackage =
        "imported-from-other-hmake-package-from-other-hmake-package";
    inline static const string includeDirectories = "include-directories";
    inline static const string index = "index";
    inline static const string infrastructureFlags = "infrastructure-flags";
    inline static const string interface = "interface";
    inline static const string libraryDependencies = "library-dependencies";
    inline static const string libraryDirectories = "library-directories";
    inline static const string libraryType = "library-type";
    inline static const string linkCommand = "link-command";
    inline static const string linkCommandColor = "link-command-color";
    inline static const string linker = "linker";
    inline static const string linkerArray = "linker-array";
    inline static const string linkerFlags = "linker-flags";
    inline static const string linkerSelectedArrayIndex = "linker-selected-array-index";
    inline static const string linkerTransitiveFlags = "linker-transitive-flags";
    inline static const string linkPrintSettings = "link-print-settings";
    inline static const string linuxUnix = "linux-unix";
    inline static const string maximumBuildThreads = "maximum-build-threads";
    inline static const string maximumLinkThreads = "maximum-link-threads";
    inline static const string msvc = "msvc";
    inline static const string name = "name";
    inline static const string objectFile = "object-file";
    inline static const string objectFiles = "object-files";
    inline static const string onlyLogicalNameOfRequireIfc = "only-logical-name-of-require-ifc";
    inline static const string outputAndErrorFiles = "output-and-error-files";
    inline static const string outputDirectory = "output-directory";
    inline static const string outputName = "output-name";
    inline static const string package = "package";
    inline static const string packageCopy = "package-copy";
    inline static const string packageCopyPath = "package-copy-path";
    inline static const string packageName = "package-name";
    inline static const string packagePath = "package-path";
    inline static const string packageVariantIndex = "package-variant-index";
    inline static const string packageVariantJson = "package-variant-json";
    inline static const string packageVersion = "package-version";
    inline static const string path = "path";
    inline static const string pathPrintLevel = "path-print-level";
    inline static const string plibraryShared = "plibrary-shared";
    inline static const string plibraryStatic = "plibrary-static";
    inline static const string postBuildCommands = "post-build-commands";
    inline static const string postBuildCommandsStatement = "post-build-commands-statement";
    inline static const string postBuildCustomCommands = "post-build-custom-commands";
    inline static const string preBuildCommands = "pre-build-commands";
    inline static const string preBuildCommandsStatement = "pre-build-commands-statement";
    inline static const string preBuildCustomCommands = "pre-build-custom-commands";
    inline static const string prebuilt = "prebuilt";
    inline static const string printColorSettings = "print-color-settings";
    inline static const string private_ = "private";
    inline static const string project = "project";
    inline static const string projectIncludeDirectories = "project-include-directories";
    inline static const string pruneCompiledSourceFileNameFromMsvcOutput =
        "prune-compiled-source-file-name-from-msvc-output";
    inline static const string pruneHeaderDependenciesFromMsvcOutput = "prune-header-dependencies-from-msvc-output";
    inline static const string public_ = "public";
    inline static const string ratioForHmakeTime = "ratio-for-hmake-time";
    inline static const string regexString = "regex-string";
    inline static const string release = "release";
    inline static const string requireIfcs = "require-ifcs";
    inline static const string shared = "shared";
    inline static const string showPercentage = "show-percentage";
    inline static const string sourceDirectory = "source-directory";
    inline static const string sourceFile = "source-file";
    inline static const string srcFile = "src-file";
    inline static const string static_ = "static";
    inline static const string targets = "targets";
    inline static const string targetsWithModules = "targets-with-modules";
    inline static const string targetType = "target-type";
    inline static const string threadId = "thread-id";
    inline static const string tool = "tool";
    inline static const string toolErrorOutput = "tool-error-output";
    inline static const string useIndex = "use-index";
    inline static const string value = "value";
    inline static const string variant = "variant";
    inline static const string variants = "variants";
    inline static const string variantsIndices = "variants-indices";
    inline static const string version = "version";
    inline static const string windows = "windows";
};
struct File
{
    path filePath;
    File() = default;
    explicit File(const path &filePath_);
};
void to_json(Json &json, const File &file);
void from_json(const Json &json, File &file);
bool operator<(const File &lhs, const File &rhs);
// TODO: Implement CMake glob like structure which will allow to define a FileArray during configure stage in one line.
struct Directory
{
    path directoryPath;
    // TODO: Will reconsider this and may delete this and provide a better API also covering resources.
    bool isCommon = false;
    unsigned long commonDirectoryNumber;
    Directory() = default;
    explicit Directory(const path &directoryPath_);
};
void to_json(Json &json, const Directory &directory);
void from_json(const Json &json, Directory &directory);
bool operator<(const Directory &lhs, const Directory &rhs);

// TODO: Thinking about changing it to PROPOGATE and NOPROPOGATE
enum class DependencyType
{
    PUBLIC,
    PRIVATE
};
void to_json(Json &j, const DependencyType &p);

struct IDD
{
    Directory includeDirectory;
    DependencyType dependencyType;
    IDD(const Directory &includeDirectory_, const DependencyType &dependencyType_);
};

struct LibraryDependency;
struct CompilerFlagsDependency
{
    string compilerFlags;
    DependencyType dependencyType;
    CompilerFlagsDependency(const string &compilerFlags_, DependencyType dependencyType_);
};

struct LinkerFlagsDependency
{
    string linkerFlags;
    DependencyType dependencyType;
};

struct CompileDefinition
{
    string name;
    string value;
    CompileDefinition() = default;
    CompileDefinition(const string &name_, const string &value_);
};

struct CompileDefinitionDependency
{
    CompileDefinition compileDefinition;
    DependencyType dependencyType;
    CompileDefinitionDependency(const CompileDefinition &compileDefinition_, DependencyType dependencyType_);
};
void to_json(Json &j, const CompileDefinition &cd);
void from_json(const Json &j, CompileDefinition &cd);

enum class Comparison
{
    EQUAL,
    GREATER_THAN,
    LESSER_THAN,
    GREATER_THAN_OR_EQUAL_TO,
    LESSER_THAN_OR_EQUAL_TO
};

struct Version
{
    unsigned majorVersion;
    unsigned minorVersion;
    unsigned patchVersion;
    Comparison comparison; // Used in flags
};
void to_json(Json &j, const Version &p);
void from_json(const Json &j, Version &v);

enum class OSFamily
{
    WINDOWS,
    LINUX_UNIX
};
void to_json(Json &json, const OSFamily &bTFamily);
void from_json(const Json &json, OSFamily &bTFamily);

enum class BTFamily
{
    GCC,
    MSVC,
    CLANG
};
void to_json(Json &json, const BTFamily &bTFamily);
void from_json(const Json &json, BTFamily &bTFamily);

struct BuildTool
{
    BTFamily bTFamily;
    Version bTVersion;
    path bTPath;
};
void to_json(Json &json, const BuildTool &buildTool);
void from_json(const Json &json, BuildTool &buildTool);

struct Compiler : BuildTool
{
};

struct Linker : BuildTool
{
};

struct Archiver : BuildTool
{
};

enum class LibraryType
{
    STATIC,
    SHARED
};
void to_json(Json &json, const LibraryType &libraryType);
void from_json(const Json &json, LibraryType &libraryType);

enum class ConfigType
{
    DEBUG,
    RELEASE
};
void to_json(Json &json, const ConfigType &configType);
void from_json(const Json &json, ConfigType &configType);

bool operator<(const Version &lhs, const Version &rhs);
bool operator==(Directory lhs, const Version &rhs);
inline bool operator>(const Version &lhs, const Version &rhs)
{
    return operator<(rhs, lhs);
}
inline bool operator<=(const Version &lhs, const Version &rhs)
{
    return !operator>(lhs, rhs);
}
inline bool operator>=(const Version &lhs, const Version &rhs)
{
    return !operator<(lhs, rhs);
}

struct CompilerVersion : public Version
{
};

// TODO: Use a JSON based approach instead. That would be more simpler, transparent and easy-to-use. Basic goal is to
// reduce the if-else for flags in configuration-files.
// TODO: Look in templatizing that. Same code is repeated in CompilerFlags and LinkerFlags
struct CompilerFlags
{
    // TODO
    //  Use variant instead of tuple
    map<tuple<BTFamily, CompilerVersion, ConfigType>, string> compilerFlags;

    set<BTFamily> compilerFamilies;
    CompilerVersion compilerVersions;
    set<ConfigType> configurationTypes;

    // bool and enum helpers for using Flags class with some operator overload magic
    mutable bool compilerHelper = false;
    mutable bool compilerVersionHelper = false;
    mutable bool configHelper = false;

    mutable BTFamily compilerCurrent;
    mutable CompilerVersion compilerVersionCurrent;
    mutable ConfigType configCurrent;

  public:
    CompilerFlags &operator[](BTFamily compilerFamily) const;
    CompilerFlags &operator[](CompilerVersion compilerVersion) const;
    CompilerFlags &operator[](ConfigType configType) const;

    void operator=(const string &flags1);
    static CompilerFlags defaultFlags();
    operator string() const;
};

struct LinkerVersion : public Version
{
};

struct LinkerFlags
{
    map<tuple<BTFamily, LinkerVersion, ConfigType>, string> linkerFlags;

    set<BTFamily> linkerFamilies;
    LinkerVersion linkerVersions;
    set<ConfigType> configurationTypes;

    // bool and enum helpers for using Flags class with some operator overload magic
    mutable bool linkerHelper = false;
    mutable bool linkerVersionHelper = false;
    mutable bool configHelper = false;

    mutable BTFamily linkerCurrent;
    mutable LinkerVersion linkerVersionCurrent;
    mutable ConfigType configCurrent;

  public:
    LinkerFlags &operator[](BTFamily linkerFamily) const;
    LinkerFlags &operator[](LinkerVersion linkerVersion) const;
    LinkerFlags &operator[](ConfigType configType) const;

    void operator=(const string &flags1);
    static LinkerFlags defaultFlags();
    operator string() const;
};

// TODO: Improve the console message.
struct Flags
{
    CompilerFlags compilerFlags = CompilerFlags::defaultFlags();
    LinkerFlags linkerFlags;
};
inline Flags flags;

struct Environment
{
    set<Directory> includeDirectories;
    set<Directory> libraryDirectories;
    string compilerFlags;
    string linkerFlags;
    static Environment initializeEnvironmentFromVSBatchCommand(const string &command);
    static Environment initializeEnvironmentOnLinux();
};
void to_json(Json &j, const Environment &environment);
void from_json(const Json &j, Environment &environment); // Used in hbuild

struct Cache
{
    static inline Directory sourceDirectory;
    static inline Directory configureDirectory;
    static inline string packageCopyPath;
    static inline bool copyPackage;
    static inline ConfigType projectConfigurationType;
    static inline vector<Compiler> compilerArray;
    static inline unsigned selectedCompilerArrayIndex;
    static inline vector<Linker> linkerArray;
    static inline unsigned selectedLinkerArrayIndex;
    static inline vector<Archiver> archiverArray;
    static inline unsigned selectedArchiverArrayIndex;
    static inline LibraryType libraryType;
    static inline Json cacheVariables;
    static inline Environment environment;
    // TODO
    // Add New Keys "OSFamily" and "TargetOSFamily" in cache.hmake
    // Not Part Of Cache Yet
#ifdef _WIN32
    static inline OSFamily osFamily = OSFamily::WINDOWS;
#else
    static inline OSFamily osFamily = OSFamily::LINUX_UNIX;
#endif
    static void initializeCache();
    static void registerCacheVariables();
};

enum class Platform
{
    LINUX,
    WINDOWS
};

class Library;

enum class VariantMode
{
    PROJECT,
    PACKAGE,
    BOTH
};

struct CustomTarget
{
    string command;
    VariantMode mode = VariantMode::PROJECT;
};

struct Variant
{
    ConfigType configurationType;
    Compiler compiler;
    Linker linker;
    Archiver archiver;
    vector<struct Executable> executables;
    vector<Library> libraries;
    Flags flags;
    LibraryType libraryType;
    Environment environment;
    Variant();
    void configure(VariantMode mode, unsigned long variantCount, const class Package &package);
};

struct ProjectVariant : public Variant
{
};

class Project
{
  public:
    string name;
    Version version;
    vector<ProjectVariant> projectVariants;
    void configure();
};

class PackageVariant : public Variant
{
  public:
    decltype(Json::object()) uniqueJson;
};

struct SourceDirectory
{
    Directory sourceDirectory;
    string regex;
    SourceDirectory() = default;
    SourceDirectory(const Directory &sourceDirectory_, const string &regex_);
};
void to_json(Json &j, const SourceDirectory &sourceDirectory);
void from_json(const Json &j, SourceDirectory &sourceDirectory);
bool operator<(const SourceDirectory &lhs, const SourceDirectory &rhs);

struct SourceAggregate // Source Directory Aggregate
{
    // User should provide the underscore at the end of the string
    string Identifier;
    set<SourceDirectory> directories;
    set<File> files;

    static inline const string sourceFilesString = JConsts::files;
    static inline const string sourceDirectoriesString = JConsts::directories;

    // If I write a to_json like other occasions and do targetFileJson = sourceAggregate; it will reinitialize the
    // object deleting the previous state.
    void convertToJson(Json &j) const;
    static set<string> convertFromJsonAndGetAllSourceFiles(const Json &j, const string &targetFilePath,
                                                           const string &Identifier = "");
    bool empty() const;
};

enum class TargetType
{
    EXECUTABLE,
    STATIC,
    SHARED,
    PLIBRARY_STATIC,
    PLIBRARY_SHARED,
};
void to_json(Json &j, const TargetType &targetType);
void from_json(const Json &j, TargetType &targetType);

string getActualNameFromTargetName(TargetType targetType, const OSFamily &osFamily, const string &targetName);
string getTargetNameFromActualName(TargetType targetType, const OSFamily &osFamily, const string &actualName);
// TODO: If no target is added in targets of variant, building that variant will be an error.
class Package;
class Target
{
  public:
    ConfigType configurationType;
    Compiler compiler;
    Linker linker;
    Archiver archiver;
    vector<IDD> includeDirectoryDependencies;
    vector<LibraryDependency> libraryDependencies;
    vector<CompilerFlagsDependency> compilerFlagsDependencies;
    vector<LinkerFlagsDependency> linkerFlagsDependencies;
    vector<CompileDefinitionDependency> compileDefinitionDependencies;
    SourceAggregate sourceAggregate{.Identifier = "SOURCE_"};
    // SourceAggregate for Modules
    SourceAggregate moduleAggregate{.Identifier = "MODULES_"};
    vector<CustomTarget> preBuild;
    vector<CustomTarget> postBuild;
    string targetName;
    string outputName;
    Directory outputDirectory;
    Environment environment;
    mutable TargetType targetType;

    // Json getVariantJson(const vector<const LibraryDependency *> &dependencies, const Package &package,
    //                   const PackageVariant &variant, unsigned count) const;
    void configure(unsigned long variantIndex) const;
    static vector<string> convertCustomTargetsToJson(const vector<CustomTarget> &customTargets, VariantMode mode);
    Json convertToJson(unsigned long variantIndex) const;
    void configure(const Package &package, unsigned count) const;
    Json convertToJson(const Package &package, unsigned count) const;
    string getTargetFilePath(unsigned long variantCount) const;
    string getTargetFilePathPackage(unsigned long variantCount) const;
    void assignDifferentVariant(const Variant &variant);

  protected:
    Target() = default;
    explicit Target(string targetName_, const Variant &variant);

    virtual ~Target() = default;
    Target(const Target & /* other */) = default;
    Target &operator=(const Target & /* other */) = default;
    Target(Target && /* other */) = default;
    Target &operator=(Target && /* other */) = default;
};

struct Executable : public Target
{
    explicit Executable(string targetName_, const Variant &variant);
    void assignDifferentVariant(const Variant &variant);
};

class Library : public Target
{
    Library() = default;
    friend struct LibraryDependency;

    LibraryType libraryType;

  public:
    explicit Library(string targetName_, const Variant &variant);
    void assignDifferentVariant(const Variant &variant);
    void setLibraryType(LibraryType libraryType_);
};

// PreBuilt-Library
class PLibrary
{
    PLibrary() = default;
    friend struct LibraryDependency;
    friend class PPLibrary;

    LibraryType libraryType;

  public:
    string libraryName;
    vector<Directory> includeDirectoryDependencies;
    vector<LibraryDependency> libraryDependencies;
    string compilerFlagsDependencies;
    string linkerFlagsDependencies;
    vector<CompileDefinition> compileDefinitionDependencies;
    path libraryPath;
    mutable TargetType targetType;
    PLibrary(const path &libraryPath_, LibraryType libraryType_);
    path getTargetVariantDirectoryPath(int variantCount) const;
    void setLibraryType(LibraryType libraryType_);
    Json convertToJson(const Package &package, unsigned count) const;
    void configure(const Package &package, unsigned count) const;
};

// ConsumePackageVariant
struct CPVariant
{
    path variantPath;
    Json variantJson;
    unsigned index;
    CPVariant(path variantPath_, Json variantJson_, unsigned index_);
};

struct CPackage;
class PPLibrary : public PLibrary
{
  private:
    friend struct LibraryDependency;
    PPLibrary() = default;

  public:
    string packageName;
    Version packageVersion;
    path packagePath;
    Json packageVariantJson;
    bool useIndex = false;
    unsigned index;
    bool importedFromOtherHMakePackage = true;
    PPLibrary(string libraryName_, const CPackage &cPackage, const CPVariant &cpVariant);
};

enum class LDLT
{ // LibraryDependencyLibraryType
    LIBRARY,
    PLIBRARY,
    PPLIBRARY
};

#include "concepts"
template <typename T>
concept HasLibraryDependencies = requires(T a) { same_as<decltype(a.libraryDependencies), vector<LibraryDependency>>; };

struct LibraryDependency
{
    // TODO: Use Variant
    Library library;
    PLibrary pLibrary;
    PPLibrary ppLibrary;

    LDLT ldlt;
    DependencyType dependencyType = DependencyType::PRIVATE;
    LibraryDependency(Library library_, DependencyType dependencyType_);
    LibraryDependency(PLibrary pLibrary_, DependencyType dependencyType_);
    LibraryDependency(PPLibrary ppLibrary_, DependencyType dependencyType_);

    template <HasLibraryDependencies Entity>
    static vector<const LibraryDependency *> getDependencies(const Entity &entity)
    {
        vector<const LibraryDependency *> dependencies;
        // This adds first layer of dependencies as is but next layers are added only if they are public.
        for (const auto &l : entity.libraryDependencies)
        {
            stack<const LibraryDependency *> st;
            st.emplace(&(l));
            while (!st.empty())
            {
                auto obj = st.top();
                st.pop();
                dependencies.emplace_back(obj);
                if (obj->ldlt == LDLT::LIBRARY)
                {
                    for (const auto &i : obj->library.libraryDependencies)
                    {
                        if (i.dependencyType == DependencyType::PUBLIC)
                        {
                            st.emplace(&(i));
                        }
                    }
                }
                else
                {
                    const PLibrary *pLib;
                    if (obj->ldlt == LDLT::PLIBRARY)
                    {
                        pLib = &(obj->pLibrary);
                    }
                    else
                    {
                        pLib = &(obj->ppLibrary);
                    }
                    for (const auto &i : pLib->libraryDependencies)
                    {
                        st.emplace(&(i));
                    }
                }
            }
        }
        return dependencies;
    }
};

class Package
{
  public:
    string name;
    Version version;
    bool cacheCommonIncludeDirs = true;
    vector<PackageVariant> packageVariants;
    vector<string> afterCopyingPackage;

    explicit Package(string name_);
    void configureCommonAmongVariants();
    void configure();

  private:
    void checkForSimilarJsonsInPackageVariants();
};

// Consume Package
struct CPackage
{
    path packagePath;
    string name;
    Version version;
    Json variantsJson;
    explicit CPackage(path packagePath_);
    CPVariant getVariant(const Json &variantJson);
    CPVariant getVariant(int index);
};

template <typename T> struct CacheVariable
{
    T value;
    string jsonString;
    CacheVariable(string cacheVariableString_, T defaultValue);
};

template <typename T>
CacheVariable<T>::CacheVariable(string cacheVariableString_, T defaultValue) : jsonString(move(cacheVariableString_))
{
    Json &cacheVariablesJson = Cache::cacheVariables;
    if (cacheVariablesJson.contains(jsonString))
    {
        value = cacheVariablesJson.at(jsonString).get<T>();
    }
    else
    {
        cacheVariablesJson["FILE1"] = defaultValue;
    }
}

enum class PathPrintLevel
{
    NO = 0,
    HALF = 1,
    FULL = 2,
};

struct PathPrint
{
    PathPrintLevel printLevel;
    unsigned depth;
    bool addQuotes;
    bool isDirectory;
    bool isTool;
};
void to_json(Json &json, const PathPrint &outputSettings);
void from_json(const Json &json, PathPrint &outputSettings);

struct CompileCommandPrintSettings
{
    PathPrint tool{
        .printLevel = PathPrintLevel::HALF, .depth = 0, .addQuotes = false, .isDirectory = false, .isTool = true};
    bool environmentCompilerFlags = false;
    bool compilerFlags = true;
    bool compilerTransitiveFlags = true;
    bool compileDefinitions = true;

    PathPrint projectIncludeDirectories{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = true, .isTool = false};
    PathPrint environmentIncludeDirectories{
        .printLevel = PathPrintLevel::NO, .depth = 1, .addQuotes = false, .isDirectory = true, .isTool = false};
    bool onlyLogicalNameOfRequireIFC = true;
    PathPrint requireIFCs{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint sourceFile{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    // Includes flags like /showIncludes /Fo /reference /ifcOutput /interface /internalPartition
    bool infrastructureFlags = false;
    PathPrint ifcOutputFile{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint objectFile{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint outputAndErrorFiles{
        .printLevel = PathPrintLevel::NO, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    bool pruneHeaderDepsFromMSVCOutput = true;
    bool pruneCompiledSourceFileNameFromMSVCOutput = true;
    bool ratioForHMakeTime = false;
    bool showPercentage = false;
};
void to_json(Json &json, const CompileCommandPrintSettings &ccpSettings);
void from_json(const Json &json, CompileCommandPrintSettings &ccpSettings);

struct ArchiveCommandPrintSettings
{
    PathPrint tool{
        .printLevel = PathPrintLevel::HALF, .depth = 0, .addQuotes = false, .isDirectory = false, .isTool = true};
    bool infrastructureFlags = true;
    PathPrint objectFiles{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint archive{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint outputAndErrorFiles{
        .printLevel = PathPrintLevel::NO, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
};
void to_json(Json &json, const ArchiveCommandPrintSettings &acpSettings);
void from_json(const Json &json, ArchiveCommandPrintSettings &acpSettings);

struct LinkCommandPrintSettings
{
    PathPrint tool{
        .printLevel = PathPrintLevel::HALF, .depth = 0, .addQuotes = false, .isDirectory = false, .isTool = true};
    bool infrastructureFlags = false;
    bool linkerFlags = true;
    bool linkerTransitiveFlags = true;
    PathPrint objectFiles{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint libraryDependencies{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint libraryDirectories{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = true, .isTool = false};
    PathPrint environmentLibraryDirectories{
        .printLevel = PathPrintLevel::NO, .depth = 3, .addQuotes = false, .isDirectory = true, .isTool = false};
    PathPrint binary{
        .printLevel = PathPrintLevel::HALF, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
    PathPrint outputAndErrorFiles{
        .printLevel = PathPrintLevel::NO, .depth = 3, .addQuotes = false, .isDirectory = false, .isTool = false};
};
void to_json(Json &json, const LinkCommandPrintSettings &lcpSettings);
void from_json(const Json &json, LinkCommandPrintSettings &lcpSettings);

struct PrintColorSettings
{
    unsigned compileCommandColor = static_cast<int>(fmt::color::light_green);
    unsigned archiveCommandColor = static_cast<int>(fmt::color::brown);
    unsigned linkCommandColor = static_cast<int>(fmt::color::pink);
    unsigned toolErrorOutput = static_cast<int>(fmt::color::red);
    unsigned hbuildStatementOutput = static_cast<int>(fmt::color::yellow);
    unsigned hbuildSequenceOutput = static_cast<int>(fmt::color::cyan);
    unsigned hbuildErrorOutput = static_cast<int>(fmt::color::orange);
};
void to_json(Json &json, const PrintColorSettings &printColorSettings);
void from_json(const Json &json, PrintColorSettings &printColorSettings);

struct GeneralPrintSettings
{
    bool preBuildCommandsStatement = true;
    bool preBuildCommands = true;
    bool postBuildCommandsStatement = true;
    bool postBuildCommands = true;
    bool copyingPackage = true;
    bool copyingTarget = true;
    bool threadId = true;
};
void to_json(Json &json, const GeneralPrintSettings &generalPrintSettings);
void from_json(const Json &json, GeneralPrintSettings &generalPrintSettings);

struct Settings
{
    unsigned int maximumBuildThreads = std::thread::hardware_concurrency();
    unsigned int maximumLinkThreads = std::thread::hardware_concurrency();

    CompileCommandPrintSettings ccpSettings;
    ArchiveCommandPrintSettings acpSettings;
    LinkCommandPrintSettings lcpSettings;
    PrintColorSettings pcSettings;
    GeneralPrintSettings gpcSettings;
};
inline Settings settings;
void to_json(Json &json, const Settings &settings_);
void from_json(const Json &json, Settings &settings_);
string file_to_string(const string &file_name);
vector<string> split(string str, const string &token);

template <same_as<char const *>... U> void ADD_PUBLIC_IDDS_TO_TARGET(Target &target, U... includeDirectoryString)
{
    (target.includeDirectoryDependencies.emplace_back(Directory{includeDirectoryString}, DependencyType::PUBLIC), ...);
}

template <same_as<char const *>... U> void ADD_PRIVATE_IDDS_TO_TARGET(Target &target, U... includeDirectoryString)
{
    (target.includeDirectoryDependencies.emplace_back(Directory{includeDirectoryString}, DependencyType::PRIVATE), ...);
}

template <same_as<Library>... U> void ADD_PUBLIC_LIB_DEPS_TO_TARGET(Target &target, const U &...libraryDependencyTarget)
{
    (target.libraryDependencies.emplace_back(libraryDependencyTarget, DependencyType::PUBLIC), ...);
}

template <same_as<Library>... U>
void ADD_PRIVATE_LIB_DEPS_TO_TARGET(Target &target, const U &...libraryDependencyTarget)
{
    (target.libraryDependencies.emplace_back(libraryDependencyTarget, DependencyType::PRIVATE), ...);
}

template <same_as<char const *> T> void ADD_PUBLIC_CFD_TO_TARGET(Target &target, T compilerFlags)
{
    target.compilerFlagsDependencies.emplace_back(compilerFlags, DependencyType::PUBLIC);
}

template <same_as<char const *> T> void ADD_PRIVATE_CFD_TO_TARGET(Target &target, T compilerFlags)
{
    target.compilerFlagsDependencies.emplace_back(compilerFlags, DependencyType::PRIVATE);
}

template <same_as<char const *> T> void ADD_PUBLIC_LFD_TO_TARGET(Target &target, T linkerFlags)
{
    target.linkerFlagsDependencies.emplace_back(linkerFlags, DependencyType::PUBLIC);
}

template <same_as<char const *> T> void ADD_PRIVATE_LFD_TO_TARGET(Target &target, T linkerFlags)
{
    target.linkerFlagsDependencies.emplace_back(linkerFlags, DependencyType::PRIVATE);
}

void ADD_PUBLIC_CDD_TO_TARGET(Target &target, const string &cddName, const string &cddValue);
void ADD_PRIVATE_CDD_TO_TARGET(Target &target, const string &cddName, const string &cddValue);

template <same_as<char const *>... U> void ADD_SRC_FILES_TO_TARGET(Target &target, U... sourceFileString)
{
    (target.sourceAggregate.files.emplace(sourceFileString), ...);
}

template <same_as<char const *>... U> void ADD_MODULE_FILES_TO_TARGET(Target &target, U... moduleFileString)
{
    (target.moduleAggregate.files.emplace(moduleFileString), ...);
}

void ADD_SRC_DIR_TO_TARGET(Target &target, const string &sourceDirectory, const string &regex);
void ADD_MODULE_DIR_TO_TARGET(Target &target, const string &moduleDirectory, const string &regex);

template <same_as<Executable>... U> void ADD_EXECUTABLES_TO_VARIANT(Variant &variant, U &...executable)
{
    (variant.executables.emplace_back(executable), ...);
}

template <same_as<Library>... U> void ADD_LIBRARIES_TO_VARIANT(Variant &variant, U &...library)
{
    (variant.libraries.emplace_back(library), ...);
}

void ADD_ENV_INCLUDES_TO_TARGET_MODULE_SRC(Target &moduleTarget);

namespace privateFunctions
{
void SEARCH_AND_ADD_FILE_FROM_ENV_INCL_TO_TARGET_MODULE_SRC(Target &moduleTarget, const string &moduleFileName);
}
template <same_as<char const *>... U>
void SEARCH_AND_ADD_FILES_FROM_ENV_INCL_TO_TARGET_MODULE_SRC(Target &moduleTarget, U... moduleFileString)
{
    (privateFunctions::SEARCH_AND_ADD_FILE_FROM_ENV_INCL_TO_TARGET_MODULE_SRC(moduleTarget, moduleFileString), ...);
}

#endif // HMAKE_CONFIGURE_HPP
