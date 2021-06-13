

#include "BBuild.hpp"
#include "fstream"
#include "iostream"

static void doJsonSpecialParseAndConvertStringArrayToPathArray(const std::string &jstr, Json &j, std::vector<fs::path> &container) {
  if (!j.contains(jstr)) {
    return;
  } else {
    if (j.at(jstr).size() == 1) {
      std::string str = j.at(jstr).get<std::string>();
      container.emplace_back(fs::path(str));
      return;
    }
    std::vector<std::string> tmpContainer;
    j.at(jstr).get_to(tmpContainer);
    for (auto &i : tmpContainer) {
      container.emplace_back(fs::path(i));
    }
  }
}

BProject::BProject(const fs::path &projectFilePath) {
  Json projectFileJson;
  std::ifstream(projectFilePath) >> projectFileJson;

  compilerPath = projectFileJson.at("COMPILER").get<Json>().at("PATH").get<std::string>();
  linkerPath = projectFileJson.at("LINKER").get<Json>().at("PATH").get<std::string>();
  compilerFlags = projectFileJson.at("COMPILER_FLAGS").get<std::string>();
  linkerFlags = projectFileJson.at("LINKER_FLAGS").get<std::string>();
  std::string libraryTypeString = projectFileJson.at("LIBRARY_TYPE").get<std::string>();
  LibraryType fileLibraryType;
  if (libraryTypeString == "STATIC") {
    fileLibraryType = LibraryType::STATIC;
  } else if (libraryTypeString == "SHARED") {
    fileLibraryType = LibraryType::SHARED;
  } else {
    throw std::runtime_error(libraryTypeString + " is not in accepted values for LIBRARY_TYPE");
  }
  libraryType = fileLibraryType;
  doJsonSpecialParseAndConvertStringArrayToPathArray("TARGETS", projectFileJson, targetFilePaths);
}

fs::path BProject::getProjectFilePathFromTargetFilePath(const fs::path &targetFilePath) {
  Json targetFileJson;
  std::ifstream(targetFilePath) >> targetFileJson;

  fs::path projectFilePath = targetFileJson.at("PROJECT_FILE_PATH").get<std::string>();
  return projectFilePath;
}

void BProject::build() {
  BTargetBuilder builer(*this);
  for (const auto &t : targetFilePaths) {
    builer.initializeTargetTypeAndParseJsonAndBuild(t);
  }
}

template<size_t N>
constexpr size_t length(char const (&)[N]) {
  return N - 1;
}

BTargetBuilder::BTargetBuilder(BProject project_)
    : project(std::move(project_)) {
}

void BTargetBuilder::initializeTargetTypeAndParseJsonAndBuild(const fs::path &targetFilePath) {

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

  Json json;
  std::ifstream(targetFilePath) >> json;
  //todo: if there is anything more specific to the executable it should be checked here. Otherwise for most parts
  // we will be defaulting to the defaults of main project file.

  //build directory assigned
  if (json.contains("BUILD_DIRECTORY")) {
    targetBuildDirectory = json.at("BUILD_DIRECTORY").get<fs::path>();
  } else {
    targetBuildDirectory = project.projectFilePath.parent_path();
  }

  //assigning source files
  if (!json.contains("SOURCE_FILES")) {
    throw std::runtime_error("Target " + targetName + " Does Not Has Any Source Files Specified in Json File.");
  } else {
    doJsonSpecialParseAndConvertStringArrayToPathArray("SOURCE_FILES", json, sourceFiles);
  }

  //assigning library dependencies
  doJsonSpecialParseAndConvertStringArrayToPathArray("LIBRARY_DEPENDENCIES", json, libraryDependencies);
  doJsonSpecialParseAndConvertStringArrayToPathArray("INCLUDE_DIRECTORIES", json, includeDirectories);

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
  build();
}

void BTargetBuilder::compileAFilePath(const fs::path &compileFileName) const {
  std::string compileCommand = project.compilerPath.string()
      + " " + project.compilerFlags + " " + compilerTransitiveFlags + " " + includeDirectoriesFlags
      + " -c " + compileFileName.string()
      + " -o " + (buildCacheFilesDirPath / compileFileName.filename()).string() + ".o";
  std::cout << compileCommand << std::endl;
  system(compileCommand.c_str());
}

void BTargetBuilder::build() {

  for (const auto &i : libraryDependencies) {

    BTargetBuilder buildTarget(this->project);
    buildTarget.initializeTargetTypeAndParseJsonAndBuild(i);
    libraryDependenciesFlags.append("-L" + buildTarget.targetBuildDirectory.string() + " -l" + buildTarget.targetName + " ");
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
      linkerCommand = project.linkerPath.string()
          + " " + project.linkerFlags + " " + linkerTransitiveFlags
          + " " + fs::path(buildCacheFilesDirPath / fs::path("")).string() + "*.o "
          + " " + libraryDependenciesFlags
          + " -o " + (targetBuildDirectory / targetNameBuildConvention).string();
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
