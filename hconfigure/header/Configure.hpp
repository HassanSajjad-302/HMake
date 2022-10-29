#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"

#ifndef HMAKE_CONFIGURE_HPP
#define HMAKE_CONFIGURE_HPP

#include "filesystem"
#include "fmt/color.h"
#include "memory"
#include "nlohmann/json.hpp"
#include "set"
#include "stack"
#include "thread"
#include "utility"

using std::filesystem::path, std::string, std::vector, std::tuple, std::map, std::set, std::same_as, std::stack,
    std::shared_ptr, std::make_shared, std::same_as;
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

enum class Dependency
{
    PUBLIC,
    PRIVATE,
    INTERFACE
};

struct LinkerFlagsDependency
{
    string linkerFlags;
};

// TODO
//  try std::tuple
struct define
{
    string name;
    string value;
    define() = default;
    explicit define(const string &name_, const string &value_ = "");
};

void to_json(Json &j, const define &cd);
void from_json(const Json &j, define &cd);

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

struct cxxflags : string
{
};

struct linkflags : string
{
};

enum class Link
{
    STATIC,
    SHARED,
};

enum class Threading
{
    SINGLE,
    MULTI
};

enum class Warnings
{
    ON,
    ALL,
    EXTRA,
    PEDANTIC,
    OFF,
};

enum class TargetOS
{
    AIX,
    ANDROID,
    APPLETV,
    BSD,
    CYGWIN,
    DARWIN,
    FREEBSD,
    HAIKU,
    HPUX,
    IPHONE,
    LINUX_,
    NETBSD,
    OPENBSD,
    OSF,
    QNX,
    QNXNTO,
    SGI,
    SOLARIS,
    UNIX_,
    UNIXWARE,
    WINDOWS,
    VMS,
    VXWORKS,
    FREERTOS
};

enum class TS
{
    GCC,
    MSVC,
    CLANG,
    DARWIN,
    PGI,
    SUN,
    GCC_3_4_4,
    GCC_4,
    GCC_4_3_4,
    GCC_4_4_0,
    GCC_4_5_0,
    GCC_4_6_0,
    GCC_4_6_3,
    GCC_4_7_0,
    GCC_4_8_0,
    GCC_5,
    DARWIN_4,
    DARWIN_5,
    PATHSCALE,
    INTEL,
    VACPP,
};
struct ToolSet
{
    string name;
    Version version;
    TS ts;

  public:
    explicit ToolSet(TS ts_ = TS::GCC);
};

struct CommonProperties
{
    Warnings warnings;
    ToolSet toolSet;
    TargetOS targetOs;
    ConfigType configurationType;
    Compiler compiler;
    Linker linker;
    Archiver archiver;
    vector<Directory> privateIncludes;
    vector<string> privateCompilerFlags;
    vector<string> privateLinkerFlags;
    vector<define> privateCompileDefinitions;
};

class Variant : public CommonProperties
{
    vector<shared_ptr<class Executable>> executablesContainer;
    vector<shared_ptr<Library>> librariesContainer;

    struct TargetComparator
    {
        bool operator()(const class Target *lhs, const class Target *rhs) const;
    };

  public:
    set<class Executable *, TargetComparator> executables;
    set<Library *, TargetComparator> libraries;
    set<class PLibrary *> preBuiltLibraries;
    set<class PPLibrary *> packagedLibraries;
    Flags flags;
    LibraryType libraryType;
    Environment environment;
    Variant();
    void copyAllTargetsFromOtherVariant(Variant &variantFrom);
    void configure(VariantMode mode, unsigned long variantCount, const class Package &package);
    Executable &findExecutable(const string &name);
    Library &findLibrary(const string &name);
    Executable &addExecutable(const string &exeName);
    Library &addLibrary(const string &libName);
    // TODO: Variant should also have functions like Target which modify the property and return the Target&.
    // These functions will return the Variant&.
};

struct ProjectVariant : public Variant
{
    explicit ProjectVariant(class Project &project);
};

class Project
{
  public:
    string name;
    Version version;
    vector<ProjectVariant *> projectVariants;
    void configure();
};

class PackageVariant : public Variant
{
  public:
    explicit PackageVariant(class Package &package);
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
class PLibrary;
class PPLibrary;

struct Dependent
{
    bool isTarget = false;
};

inline Target *targetForAndOr;
// TODO
//  Target and Executable and Library constructors should do more initializations like warnings.
class Package;
class Target : public Dependent, public CommonProperties
{
  public:
    set<Library *> publicLibs;
    set<Library *> privateLibs;
    set<PLibrary *> publicPrebuilts;
    set<PLibrary *> privatePrebuilts;
    set<PPLibrary *> publicPackagedLibs;
    set<PPLibrary *> privatePackagedLibs;

    bool targetChecked = false;

    vector<Directory> publicIncludes;
    vector<string> publicCompilerFlags;
    vector<string> publicLinkerFlags;
    vector<define> publicCompileDefinitions;
    SourceAggregate sourceAggregate{.Identifier = "SOURCE_"};
    // SourceAggregate for Modules
    SourceAggregate moduleAggregate{.Identifier = "MODULES_"};
    vector<CustomTarget> preBuild;
    vector<CustomTarget> postBuild;
    const string targetName;
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
    Json convertToJson(const Package &package, unsigned variantIndex) const;
    string getTargetFilePath(unsigned long variantCount) const;
    string getTargetFilePathPackage(unsigned long variantCount) const;

  private:
    void assignConfigurationFromVariant(const Variant &variant);
    set<Library *> getAllDependencies();

  protected:
    Target() = default;
    Target(string targetName_);
    Target(string targetName_, const Variant &variant);
    Target(string targetName_, Variant &variantFrom, Variant &variantTo, bool copyDependencies);
    virtual ~Target() = default;
    Target(const Target & /* other */) = default;
    Target &operator=(const Target & /* other */) = default;
    Target(Target && /* other */) = default;
    Target &operator=(Target && /* other */) = default;

  public:
    //

    template <same_as<char const *>... U> Target &PUBLIC_INCLUDES(U... includeDirectoryString);
    template <same_as<char const *>... U> Target &PRIVATE_INCLUDES(U... includeDirectoryString);
    template <same_as<Library *>... U> Target &PUBLIC_LIBRARIES(const U... libraries);
    template <same_as<Library *>... U> Target &PRIVATE_LIBRARIES(const U... libraries);
    Target &PUBLIC_COMPILER_FLAGS(const string &compilerFlags);
    Target &PRIVATE_COMPILER_FLAGS(const string &compilerFlags);
    Target &PUBLIC_LINKER_FLAGS(const string &linkerFlags);
    Target &PRIVATE_LINKER_FLAGS(const string &linkerFlags);
    Target &PUBLIC_COMPILE_DEFINITION(const string &cddName, const string &cddValue);
    Target &PRIVATE_COMPILE_DEFINITION(const string &cddName, const string &cddValue);
    template <same_as<char const *>... U> Target &SOURCE_FILES(U... sourceFileString);
    template <same_as<char const *>... U> Target &MODULE_FILES(U... moduleFileString);

    Target &SOURCE_DIRECTORIES(const string &sourceDirectory, const string &regex);
    Target &MODULE_DIRECTORIES(const string &moduleDirectory, const string &regex);

    inline Target &setTargetForAndOr()
    {
        targetForAndOr = this;
        return *this;
    }
    // All properties are assigned.
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Condition>
    Target &ASSIGN(T property, Condition... conditions);

    // Properties are assigned if assign bool is true. To Be Used With functions ADD and OR.
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Condition>
    Target &ASSIGN(bool assign, T property, Condition... conditions);
#define ASSIGN_I ASSIGN<Dependency::INTERFACE>
    template <typename T> bool EVALUATE(T property);

    // Eight functions are being used to manipulate properties. The two are AND and OR in global namespace.
    // AND and OR functions also have an overload that takes a target pointer and sets the global variable.
    // Remaining six include the last 4 and the 2 ASSIGN functions.

  private:
    // var left = right;
    // Only last property will be assigned. Others aren't considered.
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Condition>
    void RIGHT_MOST(T condition, Condition... conditions);

  public:
    // Multiple properties on left which are anded and after that right's assignment occurs.
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Condition>
    Target &M_LEFT_AND(T condition, Condition... conditions);
#define M_LEFT_AND_I M_LEFT_AND<Dependency::INTERFACE>

    // Single Condition and Property. M_LEFT_AND or M_LEFT_OR could be used too, but, that's more clear
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename P>
    Target &SINGLE(T condition, P property);
#define SINGLE_I SINGLE<Dependency::INTERFACE>

    // Multiple properties on left which are orred and after that right's assignment occurs.
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Condition>
    Target &M_LEFT_OR(T condition, Condition... conditions);
#define M_LEFT_OR_I M_LEFT_OR<Dependency::INTERFACE>

    // Incomplete
    // If first left succeeds then, Multiple properties of the right are assigned to the target.
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Condition>
    Target &M_RIGHT(T condition, Condition... conditions);
#define M_RIGHT_I M_RIGHT<Dependency::INTERFACE>
};

template <same_as<char const *>... U> Target &Target::PUBLIC_INCLUDES(U... includeDirectoryString)
{
    (publicIncludes.emplace_back(Directory{includeDirectoryString}), ...);
    return *this;
}

template <same_as<char const *>... U> Target &Target::PRIVATE_INCLUDES(U... includeDirectoryString)
{
    (privateIncludes.emplace_back(Directory{includeDirectoryString}...));
    return *this;
}

template <same_as<Library *>... U> Target &Target::PUBLIC_LIBRARIES(const U... libraries)
{
    (publicLibs.emplace(libraries...));
    return *this;
}

template <same_as<Library *>... U> Target &Target::PRIVATE_LIBRARIES(const U... libraries)
{
    (privateLibs.emplace(libraries...));
    return *this;
}

template <same_as<char const *>... U> Target &Target::SOURCE_FILES(U... sourceFileString)
{
    (sourceAggregate.files.emplace(sourceFileString), ...);
    return *this;
}

template <same_as<char const *>... U> Target &Target::MODULE_FILES(U... moduleFileString)
{
    (moduleAggregate.files.emplace(moduleFileString), ...);
    return *this;
}

template <Dependency dependency, typename T, typename... Condition>
Target &Target::ASSIGN(T property, Condition... conditions)
{
    if constexpr (std::is_same_v<decltype(property), TargetOS>)
    {
        targetOs = property;
        // TODO
        //  Ensure that toolset version supports it.
    }
    else if constexpr (std::is_same_v<decltype(property), ToolSet>)
    {
        toolSet = property;
        // TODO
        //  Here toolset is being assigned. It is ensured that the respective ToolSet exists in the
        //  Target's Variant. And the compiler and linker
    }
    else if constexpr (std::is_same_v<decltype(property), Warnings>)
    {
        // Beside assigning to warnings, add the respective flags for respective compilers to the
        // cxxflags.
        warnings = property;
    }
    else if constexpr (std::is_same_v<decltype(property), cxxflags>)
    {
        privateCompilerFlags.template emplace_back(property);
    }
    else if constexpr (std::is_same_v<decltype(property), linkflags>)
    {
        privateLinkerFlags.template emplace_back(property);
    }
    else if constexpr (std::is_same_v<decltype(property), define>)
    {
        privateCompileDefinitions.template emplace_back(property);
    }
    else
    {
        warnings = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
    if constexpr (sizeof...(conditions))
    {
        return ASSIGN(conditions...);
    }
    else
    {
        return *this;
    }
}

template <Dependency dependency, typename T, typename... Condition>
Target &Target::ASSIGN(bool assign, T property, Condition... conditions)
{
    return assign ? ASSIGN(property, conditions...) : *this;
}

template <typename T> bool Target::EVALUATE(T property)
{
    if constexpr (std::is_same_v<decltype(property), ToolSet>)
    {
        return toolSet.ts == property.ts;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetOS>)
    {
        return targetOs == property;
    }
    else
    {
        toolSet = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
}

template <Dependency dependency, typename T, typename... Condition>
void Target::RIGHT_MOST(T condition, Condition... conditions)
{
    if constexpr (sizeof...(conditions))
    {
        RIGHT_MOST(conditions...);
    }
    else
    {
        ASSIGN(condition);
    }
}

template <Dependency dependency, typename T, typename... Condition>
Target &Target::M_LEFT_AND(T condition, Condition... conditions)
{
    if constexpr (sizeof...(conditions))
    {
        return EVALUATE(condition) ? M_LEFT_AND(conditions...) : *this;
    }
    else
    {
        ASSIGN(condition);
        return *this;
    }
}

template <Dependency dependency, typename T, typename P> Target &Target::SINGLE(T condition, P property)
{
    if (EVALUATE(condition))
    {
        ASSIGN(property);
    }
    return *this;
}

template <Dependency dependency, typename T, typename... Condition>
Target &Target::M_LEFT_OR(T condition, Condition... conditions)
{
    if constexpr (sizeof...(conditions))
    {
        if (EVALUATE(condition))
        {
            RIGHT_MOST(conditions...);
            return *this;
        }
        return M_LEFT_OR(conditions...);
    }
    else
    {
        return *this;
    }
}

template <Dependency dependency, typename T, typename... Condition>
Target &Target::M_RIGHT(T condition, Condition... conditions)
{
    if (EVALUATE(condition))
    {
        ASSIGN(conditions...);
    }
    return *this;
}

template <typename T, typename... Condition> bool AND(T condition, Condition... conditions)
{
    if (!targetForAndOr->template EVALUATE(condition))
    {
        return false;
    }
    if constexpr (sizeof...(conditions))
    {
        return AND(conditions...);
    }
    else
    {
        return true;
    }
}

template <typename T, typename... Condition> bool OR(T condition, Condition... conditions)
{
    if (targetForAndOr->template EVALUATE(condition))
    {
        return true;
    }
    if constexpr (sizeof...(conditions))
    {
        return OR(conditions...);
    }
    else
    {
        return false;
    }
}

template <typename T, typename... Condition> bool AND(Target *target, T condition, Condition... conditions)
{
    targetForAndOr = target;
    if (!targetForAndOr->template EVALUATE(condition))
    {
        return false;
    }
    if constexpr (sizeof...(conditions))
    {
        return AND(conditions...);
    }
    else
    {
        return true;
    }
}

template <typename T, typename... Condition> bool OR(Target *target, T condition, Condition... conditions)
{
    targetForAndOr = target;
    if (targetForAndOr->template EVALUATE(condition))
    {
        return true;
    }
    if constexpr (sizeof...(conditions))
    {
        return OR(conditions...);
    }
    else
    {
        return false;
    }
}

template <> struct std::less<Target *>
{
    bool operator()(const Target *lhs, const Target *rhs) const;
};

class Executable : public Target
{
    friend class Variant;
    explicit Executable(string targetName_);

  public:
    explicit Executable(string targetName_, Variant &variant);
    Executable(string targetName_, Variant &variantFrom, Variant &variantTo, bool copyDependencies = true);
};

class Library : public Target
{
    friend class Variant;
    explicit Library(string targetName_);
    Library() = default;
    LibraryType libraryType;

  public:
    explicit Library(string targetName_, Variant &variant);
    Library(string targetName_, Variant &variantFrom, Variant &variantTo, bool copyDependencies = true);
    void setLibraryType(LibraryType libraryType_);
};

// PreBuilt-Library
class PLibrary : public Dependent
{
    PLibrary() = default;
    friend class PPLibrary;

    LibraryType libraryType;

  private:
    vector<PLibrary *> prebuilts;

  public:
    string libraryName;
    vector<Directory> includes;
    string compilerFlags;
    string linkerFlags;
    vector<define> compileDefinitions;
    path libraryPath;
    mutable TargetType targetType;
    PLibrary(Variant &variant, const path &libraryPath_, LibraryType libraryType_);
    path getTargetVariantDirectoryPath(int variantCount) const;
    void setLibraryType(LibraryType libraryType_);
    Json convertToJson(const Package &package, unsigned count) const;
    void configure(const Package &package, unsigned count) const;
};
bool operator<(const PLibrary &lhs, const PLibrary &rhs);

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
    PPLibrary() = default;
    set<PPLibrary> ppLibraries;

  public:
    string packageName;
    Version packageVersion;
    path packagePath;
    Json packageVariantJson;
    bool useIndex = false;
    unsigned index;
    bool importedFromOtherHMakePackage = true;
    PPLibrary(Variant &variant, string libraryName_, const CPackage &cPackage, const CPVariant &cpVariant);
};

enum class LDLT
{ // LibraryDependencyLibraryType
    LIBRARY,
    PLIBRARY,
    PPLIBRARY
};

class Package
{
  public:
    string name;
    Version version;
    bool cacheCommonIncludeDirs = true;
    vector<PackageVariant *> packageVariants;
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

template <typename T> class TarjanNode
{
    // Following 4 are reset in findSCCS();
    inline static int index = 0;
    inline static vector<TarjanNode *> stack;

    // Output
    inline static vector<T *> cycle;
    inline static bool cycleExists;

  public:
    inline static vector<T *> topologicalSort;

    // Input
    inline static set<TarjanNode> *tarjanNodes;

    mutable vector<TarjanNode *> deps;

    explicit TarjanNode(const T *id_);
    // Find Strongly Connected Components
    static void findSCCS();
    static void checkForCycle(string (*getStringFromTarjanNodeType)(T *t));

    const T *const id;

  private:
    void strongConnect();
    bool initialized = false;
    bool onStack;
    int nodeIndex;
    int lowLink;
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
    stack.emplace_back(this);
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
        if (tempCycle.size() > 1)
        {
            for (TarjanNode<T> *c : tempCycle)
            {
                cycle.emplace_back(const_cast<T *>(c->id));
            }
            cycleExists = true;
            return;
        }
    }
    topologicalSort.emplace_back(const_cast<T *>(id));
}

template <typename T> void TarjanNode<T>::checkForCycle(string (*getStringFromTarjanNodeType)(T *t))
{
    if (cycleExists)
    {
        fmt::print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                   "There is a Cyclic-Dependency.\n");
        unsigned int cycleSize = cycle.size();
        for (unsigned int i = 0; i < cycleSize; ++i)
        {
            if (cycleSize == i + 1)
            {
                fmt::print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                           "{} Depends On {}.\n", getStringFromTarjanNodeType(cycle[i]),
                           getStringFromTarjanNodeType(cycle[0]));
            }
            else
            {
                fmt::print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                           "{} Depends On {}.\n", getStringFromTarjanNodeType(cycle[0]),
                           getStringFromTarjanNodeType(cycle[0]));
            }
        }
        exit(EXIT_SUCCESS);
    }
}

template <typename T> bool operator<(const TarjanNode<T> &lhs, const TarjanNode<T> &rhs)
{
    return lhs.id < rhs.id;
}

template <same_as<Executable>... U> void ADD_EXECUTABLES_TO_VARIANT(Variant &variant, U &...executable)
{
    (variant.executables.emplace(executable), ...);
}

template <same_as<Library>... U> void ADD_LIBRARIES_TO_VARIANT(Variant &variant, U &...library)
{
    (variant.libraries.emplace(library), ...);
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

#pragma clang diagnostic pop