

#ifndef HMAKE_HBUILD_HEADER_BBUILDTARGET_HPP
#define HMAKE_HBUILD_HEADER_BBUILDTARGET_HPP

#include "BTargetType.hpp"
#include "nlohmann/json.hpp"
#include <filesystem>
namespace build {
using Json = nlohmann::ordered_json;
namespace fs = std::filesystem;
struct BBuildTarget {
  Json &json;
  std::string includeDirectoriesFlags;
  std::string libraryDependenciesFlags;
  fs::path buildCacheFilesDirPath;

public:
  //Following are assigned in constructor.
  fs::path targetFilePath;
  std::string targetName;
  std::string targetNameBuildConvention;
  BTargetType targetType;

  //Following are assigned in parse funcion.
  fs::path targetBuildDirectory;
  std::vector<fs::path> sourceFiles;
  std::vector<fs::path> libraryDependencies;
  std::vector<fs::path> includeDirectories;
  std::string compilerTransitiveFlags;
  std::string linkerTransitiveFlags;

  BBuildTarget(fs::path targetFilePath_, Json &json_);
  void parseJson();
  void build();
  void compileAFilePath(const fs::path &compileFileName) const;
};
}// namespace build

#endif//HMAKE_HBUILD_HEADER_BBUILDTARGET_HPP
