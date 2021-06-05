
#include "Configure.hpp"
#include "fstream"
#include "iostream"
#include "set"
#include "stack"

File::File(std::filesystem::__cxx11::path path_) {
  if (path_.is_relative()) {
    path_ = Project::SOURCE_DIRECTORY.path / path_;
  }
  if (fs::directory_entry(path_).status().type() == fs::file_type::regular) {
    path = path_;
    return;
  }
  throw std::runtime_error(path_.string() + " Is Not a Regular File");
}
Directory::Directory() {
  path = fs::current_path();
  path /= "";
}

Directory::Directory(fs::path path_) {
  path = path_.lexically_normal();
  if (path_.is_relative()) {
    path_ = Project::SOURCE_DIRECTORY.path / path_;
  }
  if (fs::directory_entry(path_).status().type() == fs::file_type::directory) {
    path = path_;
  } else {
    throw std::logic_error(path_.string() + " Is Not a directory file");
  }
  path /= "";
}

void to_json(Json &j, const DependencyType &p) {
  if (p == DependencyType::PUBLIC) {
    j = "PUBLIC";
  } else if (p == DependencyType::PRIVATE) {
    j = "PRIVATE";
  } else {
    j = "INTERFACE";
  }
}

void jsonAssignSpecialist(const std::string &jstr, Json &j, auto &container) {
  if (container.empty()) {
    return;
  }
  if (container.size() == 1) {
    j[jstr] = *container.begin();
    return;
  }
  j[jstr] = container;
}

//todo: don't give property values if they are not different from project property values.
//todo: improve property values in compile definitions.
void to_json(Json &j, const Target &target) {
  j["PROJECT_FILE_PATH"] = Project::CONFIGURE_DIRECTORY.path.string() + Project::PROJECT_NAME + ".hmake";
  j["BUILD_DIRECTORY"] = target.buildDirectoryPath.string();
  std::vector<std::string> sourceFilesArray;
  for (const auto &e : target.sourceFiles) {
    sourceFilesArray.push_back(e.path.string());
  }
  jsonAssignSpecialist("SOURCE_FILES", j, sourceFilesArray);
  //library dependencies

  std::vector<std::string> includeDirectories;
  for (const auto &e : target.includeDirectoryDependencies) {
    includeDirectories.push_back(e.includeDirectory.path.string());
  }

  std::string compilerFlags;
  for (const auto &e : target.compilerFlagsDependencies) {
    compilerFlags.append(" " + e.compilerFlags + " ");
  }

  std::string linkerFlags;
  for (const auto &e : target.linkerFlagsDependencies) {
    compilerFlags.append(" " + e.linkerFlags + " ");
  }

  Json compileDefinitionsArray;
  for (const auto &e : target.compileDefinitionDependencies) {
    Json compileDefinitionObject;
    compileDefinitionObject["NAME"] = e.compileDefinition.name;
    compileDefinitionObject["VALUE"] = e.compileDefinition.value;
    compileDefinitionsArray.push_back(compileDefinitionObject);
  }

  //This adds first layer of dependencies as is but next layers are added only if they are public.
  std::vector<const Library *> dependencies;
  for (const auto &l : target.libraryDependencies) {
    std::stack<const Library *> st;
    st.push(&(l.library));
    while (!st.empty()) {
      auto obj = st.top();
      st.pop();
      dependencies.push_back(obj);
      for (const auto &i : obj->libraryDependencies) {
        if (i.dependencyType == DependencyType::PUBLIC) {
          st.push(&(i.library));
        }
      }
    }
  }

  std::vector<std::string> libraryDependencies;
  for (auto i : dependencies) {
    for (const auto &e : i->includeDirectoryDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        std::string str = e.includeDirectory.path.string();
        includeDirectories.push_back(str);
      }
    }

    for (const auto &e : i->compilerFlagsDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        compilerFlags.append(" " + e.compilerFlags + " ");
      }
    }

    for (const auto &e : i->linkerFlagsDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        compilerFlags.append(" " + e.linkerFlags + " ");
      }
    }

    for (const auto &e : i->compileDefinitionDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        Json compileDefinitionObject;
        compileDefinitionObject["NAME"] = e.compileDefinition.name;
        compileDefinitionObject["VALUE"] = e.compileDefinition.value;
        compileDefinitionsArray.push_back(compileDefinitionObject);
      }
    }
    libraryDependencies.emplace_back(i->configureDirectory.path.string() + i->targetName + "." + [=]() {
      if (i->libraryType == LibraryType::STATIC) {
        return "static";
      } else {
        return "shared";
      }
    }() + "." + "hmake");
  }

  jsonAssignSpecialist("LIBRARY_DEPENDENCIES", j, libraryDependencies);
  jsonAssignSpecialist("INCLUDE_DIRECTORIES", j, includeDirectories);
  jsonAssignSpecialist("COMPILER_TRANSITIVE_FLAGS", j, compilerFlags);
  jsonAssignSpecialist("LINKER_TRANSITIVE_FLAGS", j, linkerFlags);
  jsonAssignSpecialist("COMPILE_DEFINITIONS", j, compileDefinitionsArray);
}

Target::Target(std::string targetName_)
    : targetName(std::move(targetName_)), configureDirectory(Project::CONFIGURE_DIRECTORY.path),
      buildDirectoryPath(Project::CONFIGURE_DIRECTORY.path) {
}

Target::Target(std::string targetName_, const fs::path &configureDirectoryPathRelativeToProjectBuildPath)
    : targetName(std::move(targetName_)) {
  auto targetConfigureDirectoryPath =
      Project::CONFIGURE_DIRECTORY.path / configureDirectoryPathRelativeToProjectBuildPath;
  fs::create_directory(targetConfigureDirectoryPath);
  configureDirectory = Directory(targetConfigureDirectoryPath);
  buildDirectoryPath = targetConfigureDirectoryPath;
}

//This will imply that directory already exists. While in the above constructor directory will be built while building
//the project.
Target::Target(std::string targetName_, Directory configureDirectory_)
    : targetName(std::move(targetName_)), buildDirectoryPath(configureDirectory_.path),
      configureDirectory(std::move(configureDirectory_)) {
}
Executable::Executable(std::string targetName_) : Target(std::move(targetName_)) {
}
Executable::Executable(std::string targetName_, const fs::path &configureDirectoryPathRelativeToProjectBuildPath)
    : Target(std::move(targetName_), configureDirectoryPathRelativeToProjectBuildPath) {
}
Executable::Executable(std::string targetName_, Directory configureDirectory_)
    : Target(std::move(targetName_), std::move(configureDirectory_)) {
}
Library::Library(std::string targetName_)
    : libraryType(Project::libraryType), Target(std::move(targetName_)) {
}
Library::Library(std::string targetName_, const fs::path &configureDirectoryPathRelativeToProjectBuildPath)
    : libraryType(Project::libraryType), Target(std::move(targetName_),
                                                configureDirectoryPathRelativeToProjectBuildPath) {
}
Library::Library(std::string targetName_, Directory configureDirectory_)
    : libraryType(Project::libraryType), Target(std::move(targetName_),
                                                std::move(configureDirectory_)) {
}

void to_json(Json &j, const Compiler &p) {
  if (p.compilerFamily == CompilerFamily::GCC) {
    j["FAMILY"] = "GCC";
  } else if (p.compilerFamily == CompilerFamily::MSVC) {
    j["FAMILY"] = "MSVC";
  } else {
    j["FAMILY"] = "CLANG";
  }
  j["PATH"] = p.path.string();
}

void to_json(Json &j, const Linker &p) {
  if (p.linkerFamily == LinkerFamily::GCC) {
    j["FAMILY"] = "GCC";
  } else if (p.linkerFamily == LinkerFamily::MSVC) {
    j["FAMILY"] = "MSVC";
  } else {
    j["FAMILY"] = "CLANG";
  }
  j["PATH"] = p.path.string();
}

Flags &Flags::operator[](CompilerFamily compilerFamily) {
  if (compileHelper || linkHelper || configHelper) {
    throw std::logic_error("Wrong Usage Of Flag Class.");
  }
  compileHelper = true;
  compilerCurrent = compilerFamily;
  return *this;
}
Flags &Flags::operator[](LinkerFamily linkerFamily) {
  if (compileHelper || linkHelper || configHelper) {
    throw std::logic_error("Wrong Usage Of Flag Class.");
  }
  linkHelper = true;
  linkerCurrent = linkerFamily;
  return *this;
}
Flags &Flags::operator[](ConfigType configType) {
  if (!compileHelper && !linkHelper) {
    throw std::logic_error("Wrong Usage Of Flag Class. First use operator[] with COMPILER_FAMILY or LINKER_FAMILY");
  }
  configHelper = true;
  configCurrent = configType;
  return *this;
}

void Flags::operator=(const std::string &flags) {
  if ((!compileHelper && !linkHelper) && !configHelper) {
    throw std::logic_error("Wrong Usage Of Flag Class.");
  }
  configHelper = false;
  if (compileHelper) {
    auto t = std::make_tuple(compilerCurrent, configCurrent);
    if (auto [pos, ok] = compilerFlags.emplace(t, flags); !ok) {
      std::cout << "Rewriting the flags in compilerFlags for this configuration";
      compilerFlags[t] = flags;
    }
    compileHelper = false;
  } else {
    auto t = std::make_tuple(linkerCurrent, configCurrent);
    if (auto [pos, ok] = linkerFlags.emplace(t, flags); !ok) {
      std::cout << "Rewriting the flags in linkerFlags for this configuration";
      linkerFlags[t] = flags;
    }
    linkHelper = false;
  }
}

Flags::operator std::string() {
  if ((!compileHelper && !linkHelper) && !configHelper) {
    throw std::logic_error("Wrong Usage Of Flag Class.");
  }
  configHelper = false;
  if (compileHelper) {
    auto t = std::make_tuple(compilerCurrent, configCurrent);
    if (compilerFlags.find(t) == compilerFlags.end() || compilerFlags[t].empty()) {
      std::cout << "No Compiler Flags Defined For This Configuration." << std::endl;
    }
    compileHelper = false;
    return compilerFlags[t];
  } else {
    auto t = std::make_tuple(linkerCurrent, configCurrent);
    if (linkerFlags.find(t) == linkerFlags.end() || linkerFlags[t].empty()) {
      std::cout << "No Linker Flags Defined For This Configuration." << std::endl;
    }
    linkHelper = false;
    return linkerFlags[t];
  }
}

PCP &PCP::operator[](Platform platform) {
  if (platformHelper || libraryTypeHelper) {
    throw std::logic_error("Wrong Usage Of PCP class");
  }
  platformHelper = true;
  platformCurrent = platform;
  return *this;
}

PCP &PCP::operator[](LibraryType libraryType) {
  if (!platformHelper) {
    throw std::logic_error("Wrong Usage of PCP class. First use operator(Platform) function");
  }
  libraryTypeHelper = true;
  libraryTypeCurrent = libraryType;
  return *this;
}

void PCP::operator=(const fs::path &copyPathRelativeToPackageVariantInstallDirectory) {
  if (!platformHelper || !libraryTypeHelper) {
    throw std::logic_error("Wrong Usage Of PCP Class.");
  }
  platformHelper = true;
  libraryTypeHelper = true;
  auto t = std::make_tuple(platformCurrent, libraryTypeCurrent);
  if (auto [pos, ok] = packageCopyPaths.emplace(t, copyPathRelativeToPackageVariantInstallDirectory); !ok) {
    std::cout << "ReWriting Install Location for this configuration" << std::endl;
    packageCopyPaths[t] = copyPathRelativeToPackageVariantInstallDirectory;
  }
}

PCP::operator fs::path() {
  if (!platformHelper || !libraryTypeHelper) {
    throw std::logic_error("Wrong Usage Of PCP Class.");
  }
  platformHelper = libraryTypeHelper = false;
  auto t = std::make_tuple(platformCurrent, libraryTypeCurrent);
  return packageCopyPaths[t];
}

void to_json(Json &j, const Version &p) {
  j = std::to_string(p.majorVersion) + "." + std::to_string(p.minorVersion) + "." + std::to_string(p.patchVersion);
}

void to_json(Json &j, const ConfigType &p) {
  if (p == ConfigType::DEBUG) {
    j = "DEBUG";
    j = "RELEASE";
  }
}

void to_json(Json &j, const Project &p) {
  j["PROJECT_NAME"] = Project::PROJECT_NAME;
  j["PROJECT_VERSION"] = Project::PROJECT_VERSION;
  j["SOURCE_DIRECTORY"] = Project::SOURCE_DIRECTORY.path.string();
  j["BUILD_DIRECTORY"] = Project::CONFIGURE_DIRECTORY.path.string();
  j["CONFIGURATION"] = Project::projectConfigurationType;
  j["COMPILER"] = Project::ourCompiler;
  j["LINKER"] = Project::ourLinker;
  std::string compilerFlags = Project::flags[Project::ourCompiler.compilerFamily][Project::projectConfigurationType];
  j["COMPILER_FLAGS"] = compilerFlags;
  std::string linkerFlags = Project::flags[Project::ourLinker.linkerFamily][Project::projectConfigurationType];
  j["LINKER_FLAGS"] = linkerFlags;

  Json exeArray = Json::array();
  for (const auto &e : Project::projectExecutables) {
    Json exeObject;
    exeObject["NAME"] = e.targetName;
    exeObject["LOCATION"] = e.configureDirectory.path.string();
    exeArray.push_back(exeObject);
  }
  jsonAssignSpecialist("EXECUTABLES", j, exeArray);

  Json libArray = Json::array();
  for (const auto &e : Project::projectLibraries) {
    Json exeObject;
    exeObject["NAME"] = e.targetName;
    exeObject["LOCATION"] = e.configureDirectory.path.string();
    exeArray.push_back(exeObject);
  }
  jsonAssignSpecialist("LIBRARIES", j, libArray);
  j["LIBRARY_TYPE"] = Project::libraryType;
}
void initializeCache(int argc, char const **argv) {
  fs::path filePath = fs::current_path() / "cache.hmake";
  if (!exists(filePath) || !is_regular_file(filePath)) {
    throw std::runtime_error(filePath.string() + " does not exists or is not a regular file");
  }
  Json cacheJson;
  std::ifstream(filePath) >> cacheJson;

  Cache::SOURCE_DIRECTORY = Directory(fs::path(cacheJson.at("SOURCE_DIRECTORY").get<std::string>()));
  Cache::CONFIGURE_DIRECTORY = Directory(fs::path(cacheJson.at("BUILD_DIRECTORY").get<std::string>()));

  std::string configTypeString = cacheJson.at("CONFIGURATION").get<std::string>();
  ConfigType configType;
  if (configTypeString == "DEBUG") {
    configType = ConfigType::DEBUG;
  } else if (configTypeString == "RELEASE") {
    configType = ConfigType::RELEASE;
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
  Project::CONFIGURE_DIRECTORY = Cache::CONFIGURE_DIRECTORY;
  Project::projectConfigurationType = Cache::projectConfigurationType;
  Project::ourCompiler = Cache::compilerArray[Cache::selectedCompilerArrayIndex];
  Project::ourLinker = Cache::linkerArray[Cache::selectedLinkerArrayIndex];
  Project::libraryType = Cache::libraryType;
  Project::hasParent = Cache::hasParent;
  Project::parentPath = Cache::parentPath;
  Project::flags[CompilerFamily::GCC][ConfigType::DEBUG] = "-g";
  Project::flags[CompilerFamily::GCC][ConfigType::RELEASE] = "-O3 -DNDEBUG";
  Project::libraryType = LibraryType::STATIC;
}

void initializeCacheAndInitializeProject(int argc, char const **argv, const std::string &projectName, Version projectVersion) {
  initializeCache(argc, argv);
  initializeProject(projectName, projectVersion);
}

void configure(const Library &library) {
  Json json;
  json = library;
  fs::path p = library.configureDirectory.path;
  std::string fileName;
  if (library.libraryType == LibraryType::STATIC) {
    fileName = library.targetName + ".static.hmake";
  } else {
    fileName = library.targetName + ".shared.hmake";
  }
  p /= fileName;
  std::ofstream(p) << json.dump(4);
  for (auto &l : library.libraryDependencies) {
    configure(l.library);
  }
}

void configure(const Executable &executable) {
  Json json;
  json = executable;
  fs::path p = executable.configureDirectory.path;
  std::string fileName = executable.targetName + ".executable.hmake";
  p /= fileName;
  std::ofstream(p) << json.dump(4);
  for (const auto &l : executable.libraryDependencies) {
    configure(l.library);
  }
}

void configure() {
  Json json = Project();
  fs::path p = Project::CONFIGURE_DIRECTORY.path;
  std::string fileName = Project::PROJECT_NAME + ".hmake";
  p /= fileName;
  std::ofstream(p) << json.dump(4);
  for (auto &t : Project::projectExecutables) {
    configure(t);
  }
  for (auto &t : Project::projectLibraries) {
    configure(t);
  }
}

void configure(const Package &package) {
  //Check that no two JSON'S of packageVariants of package are same
  std::set<nlohmann::json> checkSet;
  for (auto &i : package.packageVariants) {
    //This associatedJson is ordered_json, thus two different json's equality test will be false if the order is different even if they have same elements.
    //Thus we first convert it into json normal which is unordered one.
    nlohmann::json j = i.associatedJson;
    if (auto [pos, ok] = checkSet.emplace(j); !ok) {
      throw std::logic_error("No two json's of packagevariants can be same. Different order with same values does not make two Json's different");
    }
  }

  int count = 1;
  Json packageFileJson;
  for (auto &i : package.packageVariants) {
    packageFileJson.emplace_back(i.associatedJson);

    fs::path variantDirectoryPath = package.packageConfigureDirectory.path / std::to_string(count);
  }

  fs::path file = package.packageConfigureDirectory.path / "package.hmake";
  std::ofstream(file) << packageFileJson;
}

PackageVariant::PackageVariant() {
  packageVariantConfigurationType = Project::projectConfigurationType;
  packageVariantCompiler = Project::ourCompiler;
  packageVariantLinker = Project::ourLinker;
}

Package::Package(const fs::path &packageConfigureDirectoryPathRelativeToConfigureDirectory)
    : packageConfigureDirectory(Project::CONFIGURE_DIRECTORY.path / packageConfigureDirectoryPathRelativeToConfigureDirectory / "") {
}

SubDirectory::SubDirectory(const fs::path &subDirectorySourcePathRelativeToParentSourcePath)
    : SubDirectory(Directory(), Directory()) {
  sourceDirectory = Directory(subDirectorySourcePathRelativeToParentSourcePath);
  auto subDirectoryBuildDirectoryPath =
      Project::CONFIGURE_DIRECTORY.path / subDirectorySourcePathRelativeToParentSourcePath;
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
    cacheFileJson["PARENT_PATH"] = Project::CONFIGURE_DIRECTORY.path.string();
    cacheFileJson["USER_DEFINED"] = cacheVariablesJson;
    std::ofstream(cacheFilePath) << cacheFileJson.dump(4);
    //TODO: Here we will call the compilation command somehow.
  }
}
