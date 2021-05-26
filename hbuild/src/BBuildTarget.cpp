

#include "BBuildTarget.hpp"
#include "BProject.hpp"
#include "HBuildCustomFunctions.hpp"
#include "fstream"

build::BBuildTarget::BBuildTarget(fs::path targetFilePath_, Json &json_) : targetFilePath(std::move(targetFilePath_)), json(json_) {
  if (targetFilePath.string().ends_with(".executable.hmake")) {
    targetType = BTargetType::EXECUTABLE;
  } else if (targetFilePath.string().ends_with(".static.hmake")) {
    targetType = BTargetType::STATIC;
  } else if (targetFilePath.string().ends_with(".shared.hmake")) {
    targetType = BTargetType::SHARED;
  } else {
    throw std::runtime_error("Unknow build file path extension " + targetFilePath.string());
  }

  if (targetType == BTargetType::EXECUTABLE) {
    std::string fileName = targetFilePath.filename();
    targetName = fileName.substr(0, fileName.size() - length(".executable.hmake"));
    targetNameBuildConvention = targetName;
  } else if (targetType == BTargetType::SHARED) {
    std::string fileName = targetFilePath.filename();
    targetName = fileName.substr(0, fileName.size() - length(".shared.hmake"));
    targetNameBuildConvention = "lib" + targetName + ".so";
  } else {
    std::string fileName = targetFilePath.filename();
    targetName = fileName.substr(0, fileName.size() - length(".static.hmake"));
    targetNameBuildConvention = "lib" + targetName + ".a";
  }
  parseJson();
}

void build::BBuildTarget::parseJson() {
  //todo: if there is anything more specific to the executable it should be checked here. Otherwise for most parts
  // we will be defaulting to the defaults of main project file.

  //build directory assigned
  if (json.contains("BUILD_DIRECTORY")) {
    targetBuildDirectory = json.at("BUILD_DIRECTORY").get<fs::path>();
  } else {
    targetBuildDirectory = BProject::projectFilePath.parent_path();
  }

  //assigning source files
  if (!json.contains("SOURCE_FILES")) {
    throw std::runtime_error("Target " + targetName + " Does Not Has Any Source Files Specified in Json File.");
  } else {
    build::doJsonSpecialParseAndConvertStringArrayToPathArray("SOURCE_FILES", json, sourceFiles);
  }

  //assigning library dependencies
  build::doJsonSpecialParseAndConvertStringArrayToPathArray("LIBRARY_DEPENDENCIES", json, libraryDependencies);
  build::doJsonSpecialParseAndConvertStringArrayToPathArray("INCLUDE_DIRECTORIES", json, includeDirectories);

  if (json.contains("COMPILER_TRANSITIVE_FLAGS")) {
    json.at("COMPILER_TRANSITIVE_FLAGS").get_to(compilerTransitiveFlags);
  }
  if (json.contains("LINKER_TRANSITIVE_FLAGS")) {
    json.at("LINKER_TRANSITIVE_FLAGS").get_to(linkerTransitiveFlags);
  }

  for (const auto &i : includeDirectories) {
    includeDirectoriesFlags.append("-I " + i.string() + " ");
  }

  buildCacheFilesDirPath = targetFilePath.parent_path() / ("Cache_Build_Files_" + targetName);
}

void build::BBuildTarget::compileAFilePath(const fs::path &compileFileName) const {
  std::string compileCommand = BProject::compilerPath.string()
      + " " + BProject::compilerFlags + " " + compilerTransitiveFlags + " " + includeDirectoriesFlags
      + " -c " + compileFileName.string()
      + " -o " + (buildCacheFilesDirPath / compileFileName.filename()).string() + ".o";
  std::cout << compileCommand << std::endl;
  system(compileCommand.c_str());
}

void build::BBuildTarget::build() {

  for (const auto &i : libraryDependencies) {
    Json json1;
    std::ifstream(i) >> json1;
    BBuildTarget buildTarget(i, json1);
    libraryDependenciesFlags.append("-L" + buildTarget.targetBuildDirectory.string() + " -l" + buildTarget.targetName + " ");
    buildTarget.build();
  }
  //build process starts
  fs::create_directory(targetBuildDirectory);
  bool skipLinking = true;
  if (fs::exists(targetBuildDirectory / targetNameBuildConvention)) {
    std::cout << "Target " << targetNameBuildConvention << " Already exists. Will Check Using Cache Files Whether it needs and update." << std::endl;
    for (const auto &i : sourceFiles) {
      if (fs::last_write_time((buildCacheFilesDirPath / i.filename()).string() + ".o") < fs::last_write_time(i)) {
        std::cout << i.string() << " Needs An Updated .o cache file" << std::endl;
        compileAFilePath(i);
        skipLinking = false;
      } else {
        std::cout << "Skipping Build of file " << i.string() << std::endl;
      }
    }
  } else {
    std::cout << "Building From Start" << std::endl;
    fs::create_directory(buildCacheFilesDirPath);
    for (const auto &i : sourceFiles) {
      compileAFilePath(i);
    }
    skipLinking = false;
  }
  if (!skipLinking) {
    std::string linkerCommand;
    if (targetType == BTargetType::EXECUTABLE) {
      std::cout << "Linking" << std::endl;
      linkerCommand = BProject::linkerPath.string()
          + " " + BProject::linkerFlags + " " + linkerTransitiveFlags + " " + libraryDependenciesFlags
          + " " + fs::path(buildCacheFilesDirPath / fs::path("")).string() + "*.o " + " -o " + (targetBuildDirectory / targetNameBuildConvention).string();
    } else if (targetType == BTargetType::STATIC) {
      linkerCommand = "/usr/bin/ar rcs " + (targetBuildDirectory / fs::path(targetNameBuildConvention)).string()
          + " " + fs::path(buildCacheFilesDirPath / fs::path("")).string() + "*.o ";
    } else {
    }

    std::cout << linkerCommand << std::endl;
    system(linkerCommand.c_str());
    std::cout << "Built Complete" << std::endl;
  } else {
    std::cout << "Skipping Linking Part " << std::endl;
  }
}
