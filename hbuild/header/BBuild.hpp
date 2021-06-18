

#ifndef HMAKE_HBUILD_SRC_BBUILD_HPP
#define HMAKE_HBUILD_SRC_BBUILD_HPP

#include "filesystem"
#include "nlohmann/json.hpp"
#include "string"
#include "vector"

namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;
using JObject = decltype(Json::object());
using JArray = decltype(Json::array());

enum BTargetType {
  EXECUTABLE,
  STATIC,
  SHARED
};

struct BCopyableDependency {
  std::string path;
  bool copy;
};

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
  std::vector<BCopyableDependency> libraryDependencies;
  std::vector<BCopyableDependency> includeDirectories;
  std::string compilerTransitiveFlags;
  std::string linkerTransitiveFlags;
  std::string buildCacheFilesDirPath;

  BTargetType targetType;

  explicit BTarget(const std::string &targetFilePath);
  void build(const fs::path &copyPath = fs::path(), bool copyTarget = false);

private:
  void copy(const fs::path &copyPath);
  static void assignSpecialBCopyableDependencyVector(const std::string &jString, const Json &json,
                                                     std::vector<BCopyableDependency> &container);
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
  void buildAndCopy(const fs::path &copyPath);
  static bool shouldVariantBeCopied();
};

struct BPackage {
  explicit BPackage(const fs::path &packageFilePath);
};
#endif//HMAKE_HBUILD_SRC_BBUILD_HPP
