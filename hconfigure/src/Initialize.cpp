

#include "Initialize.hpp"
#include "Cache.hpp"
#include "Project.hpp"
#include "fstream"

void initializeCache(int argc, char const **argv) {
  fs::path filePath = fs::current_path() / "cache.hmake";
  if (!exists(filePath) || !is_regular_file(filePath)) {
    throw std::runtime_error(filePath.string() + " does not exists or is not a regular file");
  }
  Json cacheJson;
  std::ifstream(filePath) >> cacheJson;

  Cache::SOURCE_DIRECTORY = Directory(fs::path(cacheJson.at("SOURCE_DIRECTORY").get<std::string>()));
  Cache::BUILD_DIRECTORY = Directory(fs::path(cacheJson.at("BUILD_DIRECTORY").get<std::string>()));

  std::string configTypeString = cacheJson.at("CONFIGURATION").get<std::string>();
  CONFIG_TYPE configType;
  if (configTypeString == "DEBUG") {
    configType = CONFIG_TYPE::DEBUG;
  } else if (configTypeString == "RELEASE") {
    configType = CONFIG_TYPE::RELEASE;
  } else {
    throw std::runtime_error("Unknown CONFIG_TYPE " + configTypeString);
  }
  Cache::projectConfigurationType = configType;

  auto compilerArrayJson = cacheJson.at("COMPILER_ARRAY").get<decltype(nlohmann::json::array())>();
  std::vector<Compiler> compilersArray;
  for (auto i : compilerArrayJson) {
    Compiler compiler;
    compiler.path = fs::path(i.at("PATH"));
    std::string compilerFamilyString = i.at("FAMILY").get<std::string>();
    CompilerFamily compilerFamily;
    if (compilerFamilyString == "GCC") {
      compilerFamily = CompilerFamily::GCC;
    } else if (compilerFamilyString == "MSVC") {
      compilerFamily = CompilerFamily::MSVC;
    } else if (compilerFamilyString == "CLANG") {
      compilerFamily = CompilerFamily::CLANG;
    } else {
      throw std::runtime_error("Unknown COMPILER::FAMILY " + compilerFamilyString);
    }
    compiler.compilerFamily = compilerFamily;
  }
  Cache::selectedCompilerArrayIndex = cacheJson.at("COMPILER_SELECTED_ARRAY_INDEX").get<int>();

  auto linkerArrayJson = cacheJson.at("LINKER_ARRAY").get<decltype(nlohmann::json::array())>();
  std::vector<Linker> linkersArray;
  for (auto i : linkerArrayJson) {
    Linker linker;
    linker.path = fs::path(i.at("PATH"));
    std::string linkerFamilyString = i.at("FAMILY").get<std::string>();
    LinkerFamily linkerFamily;
    if (linkerFamilyString == "GCC") {
      linkerFamily = LinkerFamily::GCC;
    } else if (linkerFamilyString == "MSVC") {
      linkerFamily = LinkerFamily::MSVC;
    } else if (linkerFamilyString == "CLANG") {
      linkerFamily = LinkerFamily::CLANG;
    } else {
      throw std::runtime_error("Unknown LINKER::FAMILY " + linkerFamilyString);
    }
    linker.linkerFamily = linkerFamily;
  }
  Cache::selectedLinkerArrayIndex = cacheJson.at("COMPILER_SELECTED_ARRAY_INDEX").get<int>();

  std::string libraryTypeString = cacheJson.at("LIBRARY_TYPE").get<std::string>();
  LibraryType libraryType;
  if (libraryTypeString == "STATIC") {
    libraryType = LibraryType::STATIC;
  } else if (libraryTypeString == "SHARED") {
    libraryType = LibraryType::SHARED;
  } else {
    throw std::runtime_error("Unknown LIBRARY_TYPE " + libraryTypeString);
  }
  Cache::libraryType = libraryType;
  Cache::hasParent = cacheJson.at("HAS_PARENT").get<bool>();
  if (Cache::hasParent) {
    Cache::parentPath = cacheJson.at("PARENT_PATH").get<std::string>();
  }
}

void initializeProject(const std::string &projectName, Version projectVersion) {
  Project::PROJECT_NAME = projectName;
  Project::PROJECT_VERSION = projectVersion;
  Project::SOURCE_DIRECTORY = Cache::SOURCE_DIRECTORY;
  Project::BUILD_DIRECTORY = Cache::BUILD_DIRECTORY;
  Project::projectConfigurationType = Cache::projectConfigurationType;
  Project::ourCompiler = Cache::compilerArray[Cache::selectedCompilerArrayIndex];
  Project::ourLinker = Cache::linkerArray[Cache::selectedLinkerArrayIndex];
  Project::libraryType = Cache::libraryType;
  Project::hasParent = Cache::hasParent;
  Project::parentPath = Cache::parentPath;
  Project::flags[CompilerFamily::GCC][CONFIG_TYPE::DEBUG] = "-g";
  Project::flags[CompilerFamily::GCC][CONFIG_TYPE::RELEASE] = "-O3 -DNDEBUG";
  Project::libraryType = LibraryType::STATIC;
}

void initializeCacheAndInitializeProject(int argc, char const **argv, const std::string &projectName, Version projectVersion) {
  initializeCache(argc, argv);
  initializeProject(projectName, projectVersion);
}