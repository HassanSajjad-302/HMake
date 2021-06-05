
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

//TODO: This function should be in namespace
void jsonAssignSpecialist(const std::string &jstr, Json &j, auto &container);
struct Target {
  std::vector<IDD> includeDirectoryDependencies;
  std::vector<LibraryDependency> libraryDependencies;
  std::vector<CompilerFlagsDependency> compilerFlagsDependencies;
  std::vector<LinkerFlagsDependency> linkerFlagsDependencies;
  std::vector<CompileDefinitionDependency> compileDefinitionDependencies;
  std::vector<File> sourceFiles;
  std::string targetName;
  Directory configureDirectory;
  //todo: Why is it not Directory. Rename it to outputDirectoryPath
  fs::path buildDirectoryPath;

  //configureDirectory will be same as project::SOURCE_DIRECTORY. And the target's build directory will be
  //same as configureDirectory. To specify a different build directory, set the buildDirectoryPath variable.
  explicit Target(std::string targetName_);

  //This will create a configure directory under the project::BUILD_DIRECTORY.
  Target(std::string targetName_, const fs::path &configureDirectoryPathRelativeToProjectBuildPath);

  //TODO: This constructor should not exist. A Project's configure directory
  //should also be the configure directory for all of the targets or
  //packages.
  //This will not create the configureDirectory
  Target(std::string targetName_, Directory configureDirectory_);
};
void to_json(Json &j, const Target &target);

enum class LibraryType {
  STATIC,
  SHARED,
};

struct Executable : public Target {
  explicit Executable(std::string targetName_);
  Executable(std::string targetName_, const fs::path &configureDirectoryPathRelativeToProjectBuildPath);
  Executable(std::string targetName_, Directory configureDirectory_);
};

struct Library : public Target {
  LibraryType libraryType;
  explicit Library(std::string targetName_);
  Library(std::string targetName_, const fs::path &configureDirectoryPathRelativeToProjectBuildPath);
  Library(std::string targetName_, Directory configureDirectory_);
};

struct LibraryDependency {
  Library library;
  DependencyType dependencyType = DependencyType::PRIVATE;
};

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

void to_json(Json &j, const Compiler &p);
void to_json(Json &j, const Linker &p);

enum class ConfigType {
  DEBUG,
  RELEASE
};

//TODO: Improve the console message and documentation.
struct Flags {

  std::map<std::tuple<CompilerFamily, ConfigType>, std::string> compilerFlags;
  std::map<std::tuple<LinkerFamily, ConfigType>, std::string> linkerFlags;

  //bool and enum helpers for using Flags class with some operator overload magic
  bool compileHelper = false;
  bool linkHelper = false;
  bool configHelper = false;

  CompilerFamily compilerCurrent;
  LinkerFamily linkerCurrent;
  ConfigType configCurrent;

public:
  Flags &operator[](CompilerFamily compilerFamily);
  Flags &operator[](LinkerFamily linkerFamily);
  Flags &operator[](ConfigType configType);

  void operator=(const std::string &flags);

  operator std::string();
};

struct Cache {
  static inline Directory SOURCE_DIRECTORY;
  static inline Directory CONFIGURE_DIRECTORY;
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

//package copy paths
class PCP {
  std::map<std::tuple<Platform, LibraryType>, fs::path> packageCopyPaths;

  //bool and enum helpers for using Platform class with some operator overload magic
  bool platformHelper = false;
  bool libraryTypeHelper = false;

  Platform platformCurrent;
  LibraryType libraryTypeCurrent;

public:
  PCP &operator[](Platform platform);
  PCP &operator[](LibraryType libraryType);

  void operator=(const fs::path &copyPathRelativeToPackageVariantInstallDirectory);

  operator fs::path();
};

struct Project {
  static inline std::string PROJECT_NAME;
  static inline Version PROJECT_VERSION;
  static inline Directory SOURCE_DIRECTORY;
  static inline Directory CONFIGURE_DIRECTORY;
  static inline ConfigType projectConfigurationType;
  static inline Compiler ourCompiler;
  static inline Linker ourLinker;
  static inline std::vector<Executable> projectExecutables;
  static inline std::vector<Library> projectLibraries;
  static inline Flags flags;
  static inline LibraryType libraryType;
  static inline bool hasParent;
  static inline fs::path parentPath;
};

void to_json(Json &j, const ConfigType &p);
void to_json(Json &j, const Project &p);

void to_json(Json &j, const Version &p);

void initializeCache(int argc, const char **argv);
void initializeProject(const std::string &projectName, Version projectVersion);
void initializeCacheAndInitializeProject(int argc, char const **argv, const std::string &projectName, Version projectVersion = Version{0, 0, 0});

void configure(const Library &library);
void configure(const Executable &ourExecutable);
void configure();
class Package;
void configure(const Package &package);

struct PackageVariant {
  ConfigType packageVariantConfigurationType;
  Compiler packageVariantCompiler;
  Linker packageVariantLinker;
  Flags flags;
  LibraryType libraryType;
  std::vector<Executable> projectExecutables;
  std::vector<Library> projectLibraries;

  decltype(Json::object()) associatedJson;
  PackageVariant();
};

struct Package {
  Directory packageConfigureDirectory;
  Directory packageInstallDirectory;
  std::vector<PackageVariant> packageVariants;

  explicit Package(const fs::path &packageConfigureDirectoryPathRelativeToConfigureDirectory);
};

struct SubDirectory {
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
