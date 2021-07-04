

#ifndef HMAKE_HBUILD_SRC_BBUILD_HPP
#define HMAKE_HBUILD_SRC_BBUILD_HPP

#include "filesystem"
#include "nlohmann/json.hpp"
#include "string"

namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;

enum BTargetType {
  EXECUTABLE,
  STATIC,
  SHARED,
  PLIBRARY_STATIC,
  PLIBRARY_SHARED
};
void from_json(const Json &j, BTargetType &targetType);

struct SourceDirectory {
  fs::path sourceDirectory;
  std::string regex;
};
void from_json(const Json &j, SourceDirectory &sourceDirectory);

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
  std::string outputName;
  std::string outputDirectory;
  explicit BTarget(const std::string &targetFilePath);
};

//BuildPreBuiltTarget
struct BPTarget {
  explicit BPTarget(const std::string &targetFilePath, const fs::path &copyFrom);
};

struct BVariant {
  explicit BVariant(const fs::path &variantFilePath);
};

struct BProject {
  explicit BProject(const fs::path &projectFilePath);
};

struct BPackage {
  explicit BPackage(const fs::path &packageFilePath);
};
#endif//HMAKE_HBUILD_SRC_BBUILD_HPP
