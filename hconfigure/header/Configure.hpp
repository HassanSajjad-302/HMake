
#ifndef HMAKE_CONFIGURE_HPP
#define HMAKE_CONFIGURE_HPP

#include "filesystem"
#include "nlohmann/json.hpp"
#include "stack"
#include "utility"
#include <set>

using std::filesystem::path, std::string, std::vector, std::tuple, std::map, std::set, std::same_as, std::stack;
using Json = nlohmann::ordered_json;

std::string writePath(const path &writePath);
string addQuotes(const string &pathString);

struct File
{
    path filePath;
    explicit File(path filePath_);
};

// TODO: Implement CMake glob like structure which will allow to define a FileArray during configure stage in one line.
struct Directory
{
    path directoryPath;
    bool isCommon = false;
    int commonDirectoryNumber;
    Directory() = default;
    explicit Directory(path directoryPath_);
};
void to_json(Json &json, const Directory &directory);
void from_json(const Json &json, Directory &directory);

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
    DependencyType dependencyType = DependencyType::PRIVATE;
};

struct LibraryDependency;
struct CompilerFlagsDependency
{
    string compilerFlags;
    DependencyType dependencyType;
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
};

struct CompileDefinitionDependency
{
    CompileDefinition compileDefinition;
    DependencyType dependencyType = DependencyType::PRIVATE;
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
    int majorVersion;
    int minorVersion;
    int patchVersion;
    Comparison comparison; // Used in flags
};
void to_json(Json &j, const Version &p);
void from_json(const Json &j, Version &v);

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

// TODO: Look in templatizing that. Same code is repeated in CompilerFlags and LinkerFlags
struct CompilerFlags
{
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
    vector<Directory> includeDirectories;
    vector<Directory> libraryDirectories;
    string compilerFlags;
    string linkerFlags;
    static Environment initializeEnvironmentFromVSBatchCommand(const string &command);
};
void to_json(Json &j, const Environment &p);
void from_json(const Json &j, Environment &environment); // Used in hbuild

struct Cache
{
    static inline Directory sourceDirectory;
    static inline Directory configureDirectory;
    static inline string packageCopyPath;
    static inline bool copyPackage;
    static inline ConfigType projectConfigurationType;
    static inline vector<Compiler> compilerArray;
    static inline int selectedCompilerArrayIndex;
    static inline vector<Linker> linkerArray;
    static inline int selectedLinkerArrayIndex;
    static inline vector<Archiver> archiverArray;
    static inline int selectedArchiverArrayIndex;
    static inline LibraryType libraryType;
    static inline Json cacheVariables;
    static inline Environment environment;
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
    Json convertToJson(VariantMode mode, int variantCount);
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
    decltype(Json::object()) json;
};

struct SourceDirectory
{
    Directory sourceDirectory;
    string regex;
};

enum class TargetType
{
    EXECUTABLE,
    STATIC,
    SHARED,
    PLIBRARY_STATIC,
    PLIBRARY_SHARED,
};
void to_json(Json &j, const TargetType &p);

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
    vector<File> sourceFiles;
    vector<SourceDirectory> sourceDirectories;
    vector<CustomTarget> preBuild;
    vector<CustomTarget> postBuild;
    string targetName;
    string outputName;
    Directory outputDirectory;
    Environment environment;
    mutable TargetType targetType;

    // Json getVariantJson(const vector<const LibraryDependency *> &dependencies, const Package &package,
    //                   const PackageVariant &variant, int count) const;
    void configure(int variantIndex) const;
    static vector<string> convertCustomTargetsToJson(const vector<CustomTarget> &customTargets, VariantMode mode);
    Json convertToJson(int variantIndex) const;
    void configure(const Package &package, const PackageVariant &variant, int count) const;
    Json convertToJson(const Package &package, const PackageVariant &variant, int count) const;
    path getTargetVariantDirectoryPath(int variantCount) const;
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

// TODO: Throw in configure stage if there are no source files for Executable.
struct Executable : public Target
{
    explicit Executable(string targetName_, const Variant &variant);
    void assignDifferentVariant(const Variant &variant);
};

class Library : public Target
{
    Library() = default;
    friend struct LibraryDependency;

  public:
    LibraryType libraryType;
    explicit Library(string targetName_, const Variant &variant);
    void assignDifferentVariant(const Variant &variant);
    void setTargetType() const;
};

// PreBuilt-Library
class PLibrary
{
    PLibrary() = default;
    friend struct LibraryDependency;
    friend class PPLibrary;

  public:
    string libraryName;
    LibraryType libraryType;
    vector<Directory> includeDirectoryDependencies;
    vector<LibraryDependency> libraryDependencies;
    string compilerFlagsDependencies;
    string linkerFlagsDependencies;
    vector<CompileDefinition> compileDefinitionDependencies;
    path libraryPath;
    mutable TargetType targetType;
    PLibrary(const path &libraryPath_, LibraryType libraryType_);
    path getTargetVariantDirectoryPath(int variantCount) const;
    void setTargetType() const;
    Json convertToJson(const Package &package, const PackageVariant &variant, int count) const;
    void configure(const Package &package, const PackageVariant &variant, int count) const;
};

// ConsumePackageVariant
struct CPVariant
{
    path variantPath;
    Json variantJson;
    int index;
    CPVariant(path variantPath_, Json variantJson_, int index_);
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
    int index;
    bool imported = true;
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
concept HasLibraryDependencies = requires(T a)
{
    same_as<decltype(a.libraryDependencies), vector<LibraryDependency>>;
};

struct LibraryDependency
{
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
            st.push(&(l));
            while (!st.empty())
            {
                auto obj = st.top();
                st.pop();
                dependencies.push_back(obj);
                if (obj->ldlt == LDLT::LIBRARY)
                {
                    for (const auto &i : obj->library.libraryDependencies)
                    {
                        if (i.dependencyType == DependencyType::PUBLIC)
                        {
                            st.push(&(i));
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
                        st.push(&(i));
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
    if (cacheVariablesJson.template contains(jsonString))
    {
        value = cacheVariablesJson.at(jsonString).template get<T>();
    }
    else
    {
        cacheVariablesJson["FILE1"] = defaultValue;
    }
}

string slurp(const std::ifstream &in);
#endif // HMAKE_CONFIGURE_HPP
