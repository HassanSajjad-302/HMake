
#ifndef HMAKE_CONFIGURE_HPP
#define HMAKE_CONFIGURE_HPP

#include "filesystem"
#include "nlohmann/json.hpp"
#include "stack"
#include "utility"
#include <set>

namespace fs = std::filesystem;

struct File {
  fs::path path;
  explicit File(fs::path path);
};

//TODO: Implement CMake glob like structure which will allow to define a FileArray during configure stage in one line.
struct Directory {
  fs::path path;
  bool isCommon = false;
  int commonDirectoryNumber;
  Directory() = default;
  explicit Directory(fs::path path);
};

using Json = nlohmann::ordered_json;

//TODO: Consider something more easy like Trickle, TrickleIfLibraryTrickles, Trickle
//Public and Private are sometimes hard to reason.
enum class DependencyType {
  PUBLIC,
  PRIVATE
};
void to_json(Json &j, const DependencyType &p);

struct IDD {
  Directory includeDirectory;
  DependencyType dependencyType = DependencyType::PRIVATE;
};

struct LibraryDependency;
struct CompilerFlagsDependency {
  std::string compilerFlags;
  DependencyType dependencyType;
};
struct LinkerFlagsDependency {
  std::string linkerFlags;
  DependencyType dependencyType;
};
struct CompileDefinition {
  std::string name;
  std::string value;
};

struct CompileDefinitionDependency {
  CompileDefinition compileDefinition;
  DependencyType dependencyType = DependencyType::PRIVATE;
};
void to_json(Json &j, const CompileDefinition &cd);
void from_json(const Json &j, CompileDefinition &cd);

enum class CompilerFamily {
  GCC,
  MSVC,
  CLANG
};
void from_json(const Json &json, CompilerFamily &compilerFamily);

enum class LinkerFamily {
  GCC,
  MSVC,
  CLANG
};
void from_json(const Json &json, LinkerFamily &linkerFamily);

enum class Comparison {
  EQUAL,
  GREATER_THAN,
  LESSER_THAN,
  GREATER_THAN_OR_EQUAL_TO,
  LESSER_THAN_OR_EQUAL_TO
};

struct Version {
  int majorVersion;
  int minorVersion;
  int patchVersion;
  Comparison comparison;//Used in flags
};

struct Compiler {
  CompilerFamily compilerFamily;
  Version compilerVersion;
  fs::path path;
};
void from_json(const Json &json, Compiler &compiler);

struct Linker {
  LinkerFamily linkerFamily;
  Version linkerVersion;
  fs::path path;
};
void from_json(const Json &json, Linker &linker);

enum class LibraryType {
  STATIC,
  SHARED
};

enum class ConfigType {
  DEBUG,
  RELEASE
};

void to_json(Json &j, const Compiler &compiler);
void to_json(Json &j, const Linker &linker);
void to_json(Json &j, const LibraryType &libraryType);
void from_json(const Json &json, LibraryType &libraryType);
void to_json(Json &j, const ConfigType &configType);
void from_json(const Json &json, ConfigType &configType);

bool operator<(const Version &lhs, const Version &rhs);
bool operator==(const Version &lhs, const Version &rhs);
inline bool operator>(const Version &lhs, const Version &rhs) { return operator<(rhs, lhs); }
inline bool operator<=(const Version &lhs, const Version &rhs) { return !operator>(lhs, rhs); }
inline bool operator>=(const Version &lhs, const Version &rhs) { return !operator<(lhs, rhs); }

struct CompilerVersion : public Version {
};

//TODO: Look in templatizing that. Same code is repeated in CompilerFlags and LinkerFlags
struct CompilerFlags {
  std::map<std::tuple<CompilerFamily, CompilerVersion, ConfigType>, std::string> compilerFlags;

  std::set<CompilerFamily> compilerFamilies;
  CompilerVersion compilerVersions;
  std::set<ConfigType> configurationTypes;

  //bool and enum helpers for using Flags class with some operator overload magic
  mutable bool compilerHelper = false;
  mutable bool compilerVersionHelper = false;
  mutable bool configHelper = false;

  mutable CompilerFamily compilerCurrent;
  mutable CompilerVersion compilerVersionCurrent;
  mutable ConfigType configCurrent;

public:
  CompilerFlags &operator[](CompilerFamily compilerFamily) const;
  CompilerFlags &operator[](CompilerVersion compilerVersion) const;
  CompilerFlags &operator[](ConfigType configType) const;

  void operator=(const std::string &flags1);
  static CompilerFlags defaultFlags();
  operator std::string() const;
};

struct LinkerVersion : public Version {
};

struct LinkerFlags {
  std::map<std::tuple<LinkerFamily, LinkerVersion, ConfigType>, std::string> linkerFlags;

  std::set<LinkerFamily> linkerFamilies;
  LinkerVersion linkerVersions;
  std::set<ConfigType> configurationTypes;

  //bool and enum helpers for using Flags class with some operator overload magic
  mutable bool linkerHelper = false;
  mutable bool linkerVersionHelper = false;
  mutable bool configHelper = false;

  mutable LinkerFamily linkerCurrent;
  mutable LinkerVersion linkerVersionCurrent;
  mutable ConfigType configCurrent;

public:
  LinkerFlags &operator[](LinkerFamily linkerFamily) const;
  LinkerFlags &operator[](LinkerVersion linkerVersion) const;
  LinkerFlags &operator[](ConfigType configType) const;

  void operator=(const std::string &flags1);
  static LinkerFlags defaultFlags();
  operator std::string() const;
};

//TODO: Improve the console message.
struct Flags {
  CompilerFlags compilerFlags = CompilerFlags::defaultFlags();
  LinkerFlags linkerFlags;
};
inline Flags flags;

struct Cache {
  static inline Directory sourceDirectory;
  static inline Directory configureDirectory;
  static inline std::string packageCopyPath;
  static inline bool copyPackage;
  static inline ConfigType projectConfigurationType;
  static inline std::vector<Compiler> compilerArray;
  static inline int selectedCompilerArrayIndex;
  static inline std::vector<Linker> linkerArray;
  static inline int selectedLinkerArrayIndex;
  static inline LibraryType libraryType;
  static inline Json cacheVariables;
  static void initializeCache();
  static void registerCacheVariables();
};

enum class Platform {
  LINUX,
  MSVC,
  CLANG
};

class Executable;
class Library;

enum class VariantMode {
  PROJECT,
  PACKAGE,
  BOTH
};

struct CustomTarget {
  std::string command;
  VariantMode mode = VariantMode::PROJECT;
};

struct Variant {
  ConfigType configurationType;
  Compiler compiler;
  Linker linker;
  std::vector<Executable> executables;
  std::vector<Library> libraries;
  Flags flags;
  LibraryType libraryType;
  Variant();
  Json convertToJson(VariantMode mode, int variantCount);
};

struct ProjectVariant : public Variant {
};

class Project {
public:
  std::string name;
  Version version;
  std::vector<ProjectVariant> projectVariants;
  void configure();
};

void to_json(Json &j, const Version &p);
void from_json(const Json &j, Version &v);

class PackageVariant : public Variant {
public:
  decltype(Json::object()) json;
};

struct SourceDirectory {
  Directory sourceDirectory;
  std::string regex;
};

enum class TargetType {
  EXECUTABLE,
  STATIC,
  SHARED,
  PLIBRARY_STATIC,
  PLIBRARY_SHARED,
};
void to_json(Json &j, const TargetType &p);

//TODO: If no target is added in targets of variant, building that variant will be an error.
class Package;
class Target {
public:
  ConfigType configurationType;
  Compiler compiler;
  Linker linker;

  std::vector<IDD> includeDirectoryDependencies;
  std::vector<LibraryDependency> libraryDependencies;
  std::vector<CompilerFlagsDependency> compilerFlagsDependencies;
  std::vector<LinkerFlagsDependency> linkerFlagsDependencies;
  std::vector<CompileDefinitionDependency> compileDefinitionDependencies;
  std::vector<File> sourceFiles;
  std::vector<SourceDirectory> sourceDirectories;
  std::vector<CustomTarget> preBuild;
  std::vector<CustomTarget> postBuild;
  std::string targetName;
  std::string outputName;
  Directory outputDirectory;
  mutable TargetType targetType;

  //Json getVariantJson(const std::vector<const LibraryDependency *> &dependencies, const Package &package,
  //                  const PackageVariant &variant, int count) const;
  void configure(int variantIndex) const;
  static std::vector<std::string> convertCustomTargetsToJson(const std::vector<CustomTarget> &customTargets, VariantMode mode);
  Json convertToJson(int variantIndex) const;
  void configure(const Package &package, const PackageVariant &variant, int count) const;
  Json convertToJson(const Package &package, const PackageVariant &variant, int count) const;
  fs::path getTargetVariantDirectoryPath(int variantCount) const;
  void assignDifferentVariant(const Variant &variant);

protected:
  Target() = default;
  explicit Target(std::string targetName_, const Variant &variant);

  virtual ~Target() = default;
  Target(const Target & /* other */) = default;
  Target &operator=(const Target & /* other */) = default;
  Target(Target && /* other */) = default;
  Target &operator=(Target && /* other */) = default;
};

//TODO: Throw in configure stage if there are no source files for Executable.
struct Executable : public Target {
  explicit Executable(std::string targetName_, const Variant &variant);
  void assignDifferentVariant(const Variant &variant);
};

class LibraryDependency;
class Library : public Target {
  Library() = default;
  friend class LibraryDependency;

public:
  LibraryType libraryType;
  explicit Library(std::string targetName_, const Variant &variant);
  void assignDifferentVariant(const Variant &variant);
  void setTargetType() const;
};

//PreBuilt-Library
class PLibrary {
  PLibrary() = default;
  friend class LibraryDependency;
  friend class PPLibrary;

public:
  std::string libraryName;
  LibraryType libraryType;
  std::vector<Directory> includeDirectoryDependencies;
  std::vector<LibraryDependency> libraryDependencies;
  std::string compilerFlagsDependencies;
  std::string linkerFlagsDependencies;
  std::vector<CompileDefinition> compileDefinitionDependencies;
  fs::path libraryPath;
  mutable TargetType targetType;
  PLibrary(const fs::path &libraryPath_, LibraryType libraryType_);
  fs::path getTargetVariantDirectoryPath(int variantCount) const;
  void setTargetType() const;
  Json convertToJson(const Package &package, const PackageVariant &variant, int count) const;
  void configure(const Package &package, const PackageVariant &variant, int count) const;
};

//ConsumePackageVariant
struct CPVariant {
  fs::path variantPath;
  Json variantJson;
  int index;
  CPVariant(fs::path variantPath_, Json variantJson_, int index_);
};

class CPackage;
class PPLibrary : public PLibrary {
private:
  friend class LibraryDependency;
  PPLibrary() = default;

public:
  std::string packageName;
  Version packageVersion;
  fs::path packagePath;
  Json packageVariantJson;
  bool useIndex = false;
  int index;
  bool imported = true;
  PPLibrary(std::string libraryName_, const CPackage &cPackage, const CPVariant &cpVariant);
};

enum class LDLT {//LibraryDependencyLibraryType
  LIBRARY,
  PLIBRARY,
  PPLIBRARY
};

#include "concepts"
template<typename T>
concept HasLibraryDependencies = requires(T a) { std::same_as<decltype(a.libraryDependencies), std::vector<LibraryDependency>>; };

struct LibraryDependency {
  Library library;
  PLibrary pLibrary;
  PPLibrary ppLibrary;

  LDLT ldlt;
  DependencyType dependencyType = DependencyType::PRIVATE;
  LibraryDependency(Library library_, DependencyType dependencyType_);
  LibraryDependency(PLibrary pLibrary_, DependencyType dependencyType_);
  LibraryDependency(PPLibrary ppLibrary_, DependencyType dependencyType_);

  template<HasLibraryDependencies Entity>
  static std::vector<const LibraryDependency *> getDependencies(const Entity &entity) {
    std::vector<const LibraryDependency *> dependencies;
    //This adds first layer of dependencies as is but next layers are added only if they are public.
    for (const auto &l : entity.libraryDependencies) {
      std::stack<const LibraryDependency *> st;
      st.push(&(l));
      while (!st.empty()) {
        auto obj = st.top();
        st.pop();
        dependencies.push_back(obj);
        if (obj->ldlt == LDLT::LIBRARY) {
          for (const auto &i : obj->library.libraryDependencies) {
            if (i.dependencyType == DependencyType::PUBLIC) {
              st.push(&(i));
            }
          }
        } else {
          const PLibrary *pLib;
          if (obj->ldlt == LDLT::PLIBRARY) {
            pLib = &(obj->pLibrary);
          } else {
            pLib = &(obj->ppLibrary);
          }
          for (const auto &i : pLib->libraryDependencies) {
            st.push(&(i));
          }
        }
      }
    }
    return dependencies;
  }
};

class Package {
public:
  std::string name;
  Version version;
  bool cacheCommonIncludeDirs = true;
  std::vector<PackageVariant> packageVariants;
  std::vector<std::string> afterCopyingPackage;

  explicit Package(std::string name_);
  void configureCommonAmongVariants();
  void configure();

private:
  void checkForSimilarJsonsInPackageVariants();
};

//Consume Package
struct CPackage {
  fs::path path;
  std::string name;
  Version version;
  Json variantsJson;
  explicit CPackage(fs::path packagePath_);
  CPVariant getVariant(const Json &variantJson);
  CPVariant getVariant(const int index);
};

template<typename T>
struct CacheVariable {
  T value;
  std::string jsonString;
  CacheVariable(std::string cacheVariableString_, T defaultValue);
};

template<typename T>
CacheVariable<T>::CacheVariable(std::string cacheVariableString_, T defaultValue) : jsonString(std::move(cacheVariableString_)) {
  Json &cacheVariablesJson = Cache::cacheVariables;
  if (cacheVariablesJson.template contains(jsonString)) {
    value = cacheVariablesJson.at(jsonString).template get<T>();
  } else {
    cacheVariablesJson["FILE1"] = defaultValue;
  }
}

#endif//HMAKE_CONFIGURE_HPP
