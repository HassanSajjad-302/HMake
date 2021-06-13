

#ifndef HMAKE_HBUILD_SRC_BBUILD_HPP
#define HMAKE_HBUILD_SRC_BBUILD_HPP

#include "Configure.hpp"

class BProject {
public:
  fs::path projectFilePath;
  fs::path compilerPath;
  fs::path linkerPath;
  std::string compilerFlags;
  std::string linkerFlags;
  LibraryType libraryType;
  std::vector<fs::path> targetFilePaths;
  BProject() = default;
  explicit BProject(const fs::path &projectFilePath);
  static fs::path getProjectFilePathFromTargetFilePath(const fs::path &targetFilePath);
  void build();
};

enum BTargetType {
  EXECUTABLE,
  STATIC,
  SHARED
};

struct BTargetBuilder {
  std::string includeDirectoriesFlags;
  std::string libraryDependenciesFlags;
  fs::path buildCacheFilesDirPath;

  void build();

public:
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

  BProject project;
  explicit BTargetBuilder(BProject project);
  void initializeTargetTypeAndParseJsonAndBuild(const fs::path &targetFilePath);
  void compileAFilePath(const fs::path &compileFileName) const;
};

#endif//HMAKE_HBUILD_SRC_BBUILD_HPP
