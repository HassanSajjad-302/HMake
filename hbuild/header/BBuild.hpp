

#ifndef HMAKE_HBUILD_SRC_BBUILD_HPP
#define HMAKE_HBUILD_SRC_BBUILD_HPP

#include "filesystem"
#include "nlohmann/json.hpp"
#include "string"
#include "vector"

namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;

enum BTargetType {
  EXECUTABLE,
  STATIC,
  SHARED
};

struct BIDD {
  std::string path;
  bool copy;
};
void from_json(const Json &j, BIDD &bCompileDefinition);

struct BLibraryDependency {
  std::string path;
  std::string hmakeFilePath;
  bool copy;
  bool preBuilt;
  bool imported;
};
void from_json(const Json &j, BLibraryDependency &bCompileDefinition);

struct BCompileDefinition {
  std::string name;
  std::string value;
};
void from_json(const Json &j, BCompileDefinition &bCompileDefinition);

class BTarget {
public:
  std::string targetName;
  std::string outputName;
  std::string outputDirectory;
  std::string compilerPath;
  std::string linkerPath;
  std::string compilerFlags;
  std::string linkerFlags;
  std::vector<std::string> sourceFiles;
  std::vector<BLibraryDependency> libraryDependencies;
  std::vector<BIDD> includeDirectories;
  std::string compilerTransitiveFlags;
  std::string linkerTransitiveFlags;
  std::vector<BCompileDefinition> compileDefinitions;
  std::string buildCacheFilesDirPath;
  BTargetType targetType;
  std::string targetFileName;
  Json consumerDependenciesJson;
  fs::path packageTargetPath;
  bool isInPackage;
  bool copyPackage;
  std::string packageName;
  std::string packageCopyPath;
  int packageVariantIndex;
  explicit BTarget(const std::string &targetFilePath);
  void build();

private:
  void copy();
};

//BuildPreBuiltTarget
class BPTarget {
public:
  std::string targetName;
  std::string compilerFlags;
  std::vector<BLibraryDependency> libraryDependencies;
  std::vector<BIDD> includeDirectories;
  std::vector<BCompileDefinition> compileDefinitions;
  BTargetType targetType;
  std::string targetFileName;
  Json consumerDependenciesJson;
  fs::path packageTargetPath;
  bool isInPackage;
  bool copyPackage;
  std::string packageName;
  std::string packageCopyPath;
  int packageVariantIndex;
  explicit BPTarget(const std::string &targetFilePath);
  void build(const fs::path &copyFrom);

private:
  void copy(fs::path copyFrom);
};

struct BVariant {
  std::vector<std::string> targetFilePaths;
  explicit BVariant(const fs::path &variantFilePath);
  void build();
};
struct BProjectVariant : BVariant {
  explicit BProjectVariant(const fs::path &projectFilePath);
};

struct BPackageVariant : BVariant {
  explicit BPackageVariant(const fs::path &packageVariantFilePath);
};

struct BPackage {
  explicit BPackage(const fs::path &packageFilePath);
};
#endif//HMAKE_HBUILD_SRC_BBUILD_HPP
