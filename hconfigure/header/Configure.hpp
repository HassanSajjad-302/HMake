
#ifndef HMAKE_CONFIGURE_HPP
#define HMAKE_CONFIGURE_HPP

#include "filesystem"
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;

struct File {
  fs::path path;
  explicit File(fs::path path);
};

//TODO: Implement CMake glob like structure which will allow to define a FileArray during configure stage in one line.
struct Directory {
  fs::path path;
  Directory();
  explicit Directory(fs::path path);
};

using Json = nlohmann::ordered_json;
using JObject = decltype(Json::object());
using JArray = decltype(Json::array());

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
void to_json(Json &j, const CompileDefinitionDependency &cdd);

enum class CompilerFamily {
  ANY,
  GCC,
  MSVC,
  CLANG
};

enum class LinkerFamily {
  ANY,
  GCC,
  MSVC,
  CLANG
};

struct Compiler {
  CompilerFamily compilerFamily;
  fs::path path;
};

struct Linker {
  LinkerFamily linkerFamily;
  fs::path path;
};

enum class LibraryType {
  STATIC,
  SHARED,
};

enum class ConfigType {
  DEBUG,
  RELEASE
};

void to_json(Json &j, const Compiler &compiler);
void to_json(Json &j, const Linker &linker);
void to_json(Json &j, LibraryType libraryType);
void to_json(Json &j, ConfigType configType);

//TODO: Improve the console message and documentation.
struct Flags {

  std::map<std::tuple<CompilerFamily, ConfigType>, std::string> compilerFlags;
  std::map<std::tuple<LinkerFamily, ConfigType>, std::string> linkerFlags;

  //bool and enum helpers for using Flags class with some operator overload magic
  mutable bool compileHelper = false;
  mutable bool linkHelper = false;
  mutable bool configHelper = false;

  mutable CompilerFamily compilerCurrent;
  mutable LinkerFamily linkerCurrent;
  mutable ConfigType configCurrent;

public:
  Flags &operator[](CompilerFamily compilerFamily) const;
  Flags &operator[](LinkerFamily linkerFamily) const;
  Flags &operator[](ConfigType configType) const;

  void operator=(const std::string &flags);
  static Flags defaultFlags();
  operator std::string() const;
};
inline Flags flags;

struct Cache {
  static inline Directory sourceDirectory;
  static inline Directory configureDirectory;
  static inline ConfigType projectConfigurationType;
  static inline std::vector<Compiler> compilerArray;
  static inline int selectedCompilerArrayIndex;
  static inline std::vector<Linker> linkerArray;
  static inline int selectedLinkerArrayIndex;
  static inline LibraryType libraryType;
  static inline Json cacheJson;
  static inline bool hasParent;
  static inline fs::path parentPath;
};

struct Version {
  int majorVersion{};
  int minorVersion{};
  int patchVersion{};
};

enum class Platform {
  LINUX,
  MSVC,
  CLANG
};

//Package Copy Category
enum class PCCategory {
  EXECUTABLE,
  STATIC,
  SHARED,
  INCLUDE
};

class Executable;
class Library;
struct Project {
  std::string name;
  Version version;
  Directory configureDirectory;
  ConfigType projectConfigurationType;
  Compiler compiler;
  Linker linker;
  std::vector<Executable> executables;
  std::vector<Library> libraries;
  Flags flags;
  LibraryType libraryType;
  bool hasParent;
  fs::path parentPath;

  explicit Project(std::string name_, Version version = Version{});
  Json convertToJson();
  void configure();
};

void to_json(Json &j, const Project &project);
void to_json(Json &j, const Version &p);

void initializeCache(int argc, const char **argv);
Project initializeProject(const std::string &projectName, Version projectVersion);
void initializeCacheAndInitializeProject(int argc, char const **argv, const std::string &projectName, Version projectVersion = Version{0, 0, 0});

class PackageVariant {
public:
  ConfigType configurationType;
  Compiler compiler;
  Linker linker;
  Flags flags;
  LibraryType libraryType;
  std::vector<Executable> executables;
  std::vector<Library> libraries;

  decltype(Json::object()) json;
  PackageVariant();
  //TODO: This should be friend of Project and method should not be public because it is called by configure
  //method while PackageVariant don't have configure method.
  Json convertToJson(const Directory &packageConfigureDirectory, int count);
};

class Package;
class Target {
public:
  Project project;
  std::vector<IDD> includeDirectoryDependencies;
  std::vector<LibraryDependency> libraryDependencies;
  std::vector<CompilerFlagsDependency> compilerFlagsDependencies;
  std::vector<LinkerFlagsDependency> linkerFlagsDependencies;
  std::vector<CompileDefinitionDependency> compileDefinitionDependencies;
  std::vector<File> sourceFiles;
  std::string targetName;
  std::string outputName;
  Directory outputDirectory;

  std::vector<const LibraryDependency *> getDependencies() const;
  //Json getVariantJson(const std::vector<const LibraryDependency *> &dependencies, const Package &package,
  //                  const PackageVariant &variant, int count) const;
  void configure() const;
  Json convertToJson() const;
  void configure(const Package &package, const PackageVariant &variant, int count) const;
  Json convertToJson(const Package &package, const PackageVariant &variant, int count) const;
  fs::path getTargetConfigureDirectoryPath() const;
  fs::path getTargetVariantDirectoryPath(int variantCount) const;

protected:
  explicit Target(std::string targetName_, Project project_);

  virtual ~Target() = default;
  Target(const Target & /* other */) = default;
  Target &operator=(const Target & /* other */) = default;
  Target(Target && /* other */) = default;
  Target &operator=(Target && /* other */) = default;

  virtual std::string getFileName() const = 0;
};

//TODO: Throw in configure stage if there are no source files for Executable.
struct Executable : public Target {
  explicit Executable(std::string targetName_, Project project_);
  std::string getFileName() const override;
};

struct Library : public Target {
  LibraryType libraryType;
  explicit Library(std::string targetName_, Project project_);
  std::string getFileName() const override;
};

struct LibraryDependency {
  Library library;
  DependencyType dependencyType = DependencyType::PRIVATE;
};

class Package {
public:
  Directory packageInstallDirectory;
  std::vector<PackageVariant> packageVariants;

  explicit Package();
  void configure();

private:
  void checkForSimilarJsonsInPackageVariants();
};

class SubDirectory {
  Directory sourceDirectory;
  Directory buildDirectory;
  ConfigType projectConfigurationType;
  std::vector<Compiler> compilerArray;
  int selectedCompilerArrayIndex;
  std::vector<Linker> linkerArray;
  int selectedLinkerArrayIndex;
  LibraryType libraryType;
  Json cacheVariablesJson;

  explicit SubDirectory(const fs::path &subDirectorySourcePathRelativeToParentSourcePath);
  SubDirectory(Directory sourceDirectory_, Directory buildDirectory_);
  void configure();
};
#endif//HMAKE_CONFIGURE_HPP
