

#include "SubDirectory.hpp"
#include "fstream"
#include <Cache.hpp>
#include <Project.hpp>
#include <utility>

SubDirectory::SubDirectory(const fs::path &subDirectorySourcePathRelativeToParentSourcePath)
    : SubDirectory(Directory(), Directory()) {
  sourceDirectory = Directory(subDirectorySourcePathRelativeToParentSourcePath);
  auto subDirectoryBuildDirectoryPath =
      Project::BUILD_DIRECTORY.path / subDirectorySourcePathRelativeToParentSourcePath;
  fs::create_directory(subDirectoryBuildDirectoryPath);
  buildDirectory = Directory(subDirectoryBuildDirectoryPath);
}

SubDirectory::SubDirectory(Directory sourceDirectory_, Directory buildDirectory_)
    : sourceDirectory(std::move(sourceDirectory_)), buildDirectory(std::move(buildDirectory_)),
      projectConfigurationType(Cache::projectConfigurationType), compilerArray(Cache::compilerArray),
      selectedCompilerArrayIndex(Cache::selectedCompilerArrayIndex), linkerArray(Cache::linkerArray),
      selectedLinkerArrayIndex(Cache::selectedLinkerArrayIndex), libraryType(Cache::libraryType) {
}

void SubDirectory::configure() {
  int count = 0;
  fs::path cacheFilePath;
  fs::path cp = buildDirectory.path;
  for (const auto &i : fs::directory_iterator(cp)) {
    if (i.is_regular_file() && i.path().filename() == "cache.hmake") {
      cacheFilePath = i.path();
      ++count;
    }
  }
  if (count > 1) {
    throw std::runtime_error("More than one file with cache.hmake name present");
  }
  if (count == 0) {
    Json cacheFileJson;
    std::vector<Compiler> compilersDetected;
    compilersDetected.push_back(Compiler{CompilerFamily::GCC, fs::path("/usr/bin/g++")});
    std::vector<Linker> linkersDetected;
    linkersDetected.push_back(Linker{LinkerFamily::GCC, fs::path("/usr/bin/g++")});
    cacheFileJson["SOURCE_DIRECTORY"] = sourceDirectory.path.string();
    cacheFileJson["BUILD_DIRECTORY"] = buildDirectory.path.string();
    cacheFileJson["CONFIGURATION"] = projectConfigurationType;
    cacheFileJson["COMPILER_ARRAY"] = compilerArray;
    cacheFileJson["COMPILER_SELECTED_ARRAY_INDEX"] = selectedCompilerArrayIndex;
    cacheFileJson["LINKER_ARRAY"] = linkerArray;
    cacheFileJson["LINKER_SELECTED_ARRAY_INDEX"] = selectedLinkerArrayIndex;
    cacheFileJson["LIBRARY_TYPE"] = libraryType;
    cacheFileJson["HAS_PARENT"] = true;
    cacheFileJson["PARENT_PATH"] = Project::BUILD_DIRECTORY.path.string();
    cacheFileJson["USER_DEFINED"] = cacheVariablesJson;
    std::ofstream(cacheFilePath) << cacheFileJson.dump(4);
    //TODO: Here we will call the compilation command somehow.
  }
}
