
#include "Configure.hpp"
#include "fstream"
#include "iostream"
#include "set"
#include "stack"

File::File(std::filesystem::__cxx11::path path_) {
  if (path_.is_relative()) {
    path_ = Project::sourceDirectory.path / path_;
  }
  if (fs::directory_entry(path_).status().type() == fs::file_type::regular) {
    path = path_.lexically_normal();
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
    path_ = Project::sourceDirectory.path / path_;
  }
  if (fs::directory_entry(path_).status().type() == fs::file_type::directory) {
    path = path_.lexically_normal();
  } else {
    throw std::logic_error(path_.string() + " Is Not a directory file");
  }
  path /= "";
}

void to_json(Json &j, const DependencyType p) {
  if (p == DependencyType::PUBLIC) {
    j = "PUBLIC";
  } else if (p == DependencyType::PRIVATE) {
    j = "PRIVATE";
  } else {
    j = "INTERFACE";
  }
}

void to_json(Json &j, const CompileDefinitionDependency &cdd) {
  j["NAME"] = cdd.compileDefinition.name;
  j["VALUE"] = cdd.compileDefinition.value;
}

void to_json(Json &j, const LibraryType libraryType) {
  if (libraryType == LibraryType::STATIC) {
    j = "STATIC";
  } else {
    j = "SHARED";
  }
}

void to_json(Json &j, const ConfigType configType) {
  if (configType == ConfigType::DEBUG) {
    j = "DEBUG";
  } else {
    j = "RELEASE";
  }
}

static void jsonAssignSpecialist(const std::string &jstr, Json &j, auto &container) {
  if (container.empty()) {
    return;
  }
  if (container.size() == 1) {
    j[jstr] = *container.begin();
    return;
  }
  j[jstr] = container;
}

Target::Target(std::string targetName_)
    : targetName(std::move(targetName_)), configureDirectory(Project::configureDirectory),
      buildOutputDirectory(Project::configureDirectory), configureDirectoryPathRelativeToPV(fs::path(targetName + "/")) {
}

Target::Target(std::string targetName_, const fs::path &configureDirectoryPathRelativeToProjectBuildPath)
    : targetName(std::move(targetName_)), configureDirectoryPathRelativeToPV(fs::path(targetName + "/")) {
  auto targetConfigureDirectoryPath =
      Project::configureDirectory.path / configureDirectoryPathRelativeToProjectBuildPath;
  fs::create_directory(targetConfigureDirectoryPath);
  configureDirectory = Directory(targetConfigureDirectoryPath);
  buildOutputDirectory = configureDirectory;
}

std::vector<const LibraryDependency *> Target::getDependencies() const {
  std::vector<const LibraryDependency *> dependencies;
  //This adds first layer of dependencies as is but next layers are added only if they are public.
  for (const auto &l : libraryDependencies) {
    std::stack<const LibraryDependency *> st;
    st.push(&(l));
    while (!st.empty()) {
      auto obj = st.top();
      st.pop();
      dependencies.push_back(obj);
      for (const auto &i : obj->library.libraryDependencies) {
        if (i.dependencyType == DependencyType::PUBLIC) {
          st.push(&(i));
        }
      }
    }
  }
  return dependencies;
}

//todo: don't give property values if they are not different from project property values.
//todo: improve property values in compile definitions.
Json Target::convertToJson() const {
  Json targetFileJson;

  std::vector<std::string> sourceFilesArray;
  std::vector<std::string> includeDirectories;
  std::string compilerFlags;
  std::string linkerFlags;
  Json compileDefinitionsArray;
  std::vector<std::string> dependenciesString;

  for (const auto &e : sourceFiles) {
    sourceFilesArray.push_back(e.path.string());
  }
  for (const auto &e : includeDirectoryDependencies) {
    includeDirectories.push_back(e.includeDirectory.path.string());
  }
  for (const auto &e : compilerFlagsDependencies) {
    compilerFlags.append(" " + e.compilerFlags + " ");
  }
  for (const auto &e : linkerFlagsDependencies) {
    linkerFlags.append(" " + e.linkerFlags + " ");
  }
  for (const auto &e : compileDefinitionDependencies) {
    Json compileDefinitionObject = e;
    compileDefinitionsArray.push_back(compileDefinitionObject);
  }

  std::vector<const LibraryDependency *> dependencies;
  dependencies = getDependencies();
  for (auto libraryDependency : dependencies) {
    const Library &library = libraryDependency->library;
    for (const auto &e : library.includeDirectoryDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        std::string str = e.includeDirectory.path.string();
        includeDirectories.push_back(str);
      }
    }

    for (const auto &e : library.compilerFlagsDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        compilerFlags.append(" " + e.compilerFlags + " ");
      }
    }

    for (const auto &e : library.linkerFlagsDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        linkerFlags.append(" " + e.linkerFlags + " ");
      }
    }

    for (const auto &e : library.compileDefinitionDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        Json compileDefinitionObject = e;
        compileDefinitionsArray.push_back(compileDefinitionObject);
      }
    }
    dependenciesString.emplace_back((library.configureDirectory.path / fs::path(library.getFileName())).string());
  }

  targetFileJson["PROJECT_FILE_PATH"] = Project::configureDirectory.path.string() + Project::name + ".hmake";
  jsonAssignSpecialist("SOURCE_FILES", targetFileJson, sourceFilesArray);
  jsonAssignSpecialist("LIBRARY_DEPENDENCIES", targetFileJson, dependenciesString);
  jsonAssignSpecialist("INCLUDE_DIRECTORIES", targetFileJson, includeDirectories);
  jsonAssignSpecialist("COMPILER_TRANSITIVE_FLAGS", targetFileJson, compilerFlags);
  jsonAssignSpecialist("LINKER_TRANSITIVE_FLAGS", targetFileJson, linkerFlags);
  jsonAssignSpecialist("COMPILE_DEFINITIONS", targetFileJson, compileDefinitionsArray);
  return targetFileJson;
}

void Target::configure() const {
  Json targetFileJson;
  targetFileJson = convertToJson();
  fs::path p = configureDirectory.path;
  p /= getFileName();
  std::ofstream(p) << targetFileJson.dump(4);
  for (const auto &libDep : libraryDependencies) {
    libDep.library.configure();
  }
}

//todo: don't give property values if they are not different from project property values.
//todo: improve property values in compile definitions.
Json Target::convertToJson(const Package &package, const PackageVariant &variant, int count) const {
  Json targetFileJson;

  std::vector<std::string> sourceFilesArray;
  JArray includeDirectoriesArray;
  std::string compilerFlags;
  std::string linkerFlags;
  JArray compileDefinitionsArray;
  JArray dependenciesArray;

  std::vector<std::string> consumerIncludeDirectories;
  std::string consumerCompilerFlags;
  std::string consumerLinkerFlags;
  JArray consumerCompileDefinitionsArray;
  JArray consumerDependenciesArray;

  for (const auto &e : sourceFiles) {
    sourceFilesArray.push_back(e.path.string());
  }
  for (const auto &e : includeDirectoryDependencies) {
    JObject JIDDObject;
    JIDDObject["PATH"] = e.includeDirectory.path.string();
    if (e.dependencyType == DependencyType::PUBLIC) {
      JIDDObject["COPY"] = true;
      consumerIncludeDirectories.emplace_back("include/");
    }
    includeDirectoriesArray.push_back(JIDDObject);
  }
  for (const auto &e : compilerFlagsDependencies) {
    compilerFlags.append(" " + e.compilerFlags + " ");
    if (e.dependencyType == DependencyType::PUBLIC) {
      consumerCompilerFlags.append(" " + e.compilerFlags + " ");
    }
  }
  for (const auto &e : linkerFlagsDependencies) {
    linkerFlags.append(" " + e.linkerFlags + " ");
    if (e.dependencyType == DependencyType::PUBLIC) {
      consumerLinkerFlags.append(" " + e.linkerFlags + " ");
    }
  }
  for (const auto &e : compileDefinitionDependencies) {
    Json compileDefinitionObject = e;
    compileDefinitionsArray.push_back(compileDefinitionObject);
    if (e.dependencyType == DependencyType::PUBLIC) {
      consumerCompileDefinitionsArray.push_back(compileDefinitionObject);
    }
  }

  std::vector<const LibraryDependency *> dependencies;
  dependencies = getDependencies();
  for (auto libraryDependency : dependencies) {
    const Library &library = libraryDependency->library;
    for (const auto &e : library.includeDirectoryDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        JObject JIDDObject;
        JIDDObject["PATH"] = e.includeDirectory.path.string();
        includeDirectoriesArray.push_back(JIDDObject);
        consumerIncludeDirectories.push_back("../" + library.targetName + "/include/");
      }
    }

    for (const auto &e : library.compilerFlagsDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        compilerFlags.append(" " + e.compilerFlags + " ");
        consumerCompilerFlags.append(" " + e.compilerFlags + " ");
      }
    }

    for (const auto &e : library.linkerFlagsDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        linkerFlags.append(" " + e.linkerFlags + " ");
        consumerLinkerFlags.append(" " + e.linkerFlags + " ");
      }
    }

    for (const auto &e : library.compileDefinitionDependencies) {
      if (e.dependencyType == DependencyType::PUBLIC) {
        Json compileDefinitionObject = e;
        compileDefinitionsArray.push_back(compileDefinitionObject);
        consumerCompileDefinitionsArray.push_back(compileDefinitionObject);
      }
    }

    JObject libDepObject;
    fs::path targetFileBuildDir = package.packageConfigureDirectory.path / fs::path(std::to_string(count)) / fs::path(library.configureDirectoryPathRelativeToPV) / library.getFileName();
    consumerDependenciesArray.push_back("../" + library.targetName + "/" + library.getFileName());
    libDepObject["PATH"] = targetFileBuildDir;
    dependenciesArray.push_back(libDepObject);
  }

  targetFileJson["VARIANT_FILE_PATH"] = Project::configureDirectory.path.string() + Project::name + ".hmake";

  jsonAssignSpecialist("SOURCE_FILES", targetFileJson, sourceFilesArray);
  jsonAssignSpecialist("LIBRARY_DEPENDENCIES", targetFileJson, dependenciesArray);
  jsonAssignSpecialist("INCLUDE_DIRECTORIES", targetFileJson, includeDirectoriesArray);
  jsonAssignSpecialist("COMPILER_TRANSITIVE_FLAGS", targetFileJson, compilerFlags);
  jsonAssignSpecialist("LINKER_TRANSITIVE_FLAGS", targetFileJson, linkerFlags);
  jsonAssignSpecialist("COMPILE_DEFINITIONS", targetFileJson, compileDefinitionsArray);

  Json consumerDependenciesJson;
  jsonAssignSpecialist("LIBRARY_DEPENDENCIES", consumerDependenciesJson, consumerDependenciesArray);
  jsonAssignSpecialist("INCLUDE_DIRECTORIES", consumerDependenciesJson, consumerIncludeDirectories);
  jsonAssignSpecialist("COMPILER_TRANSITIVE_FLAGS", consumerDependenciesJson, consumerCompilerFlags);
  jsonAssignSpecialist("LINKER_TRANSITIVE_FLAGS", consumerDependenciesJson, consumerLinkerFlags);
  jsonAssignSpecialist("COMPILE_DEFINITIONS", consumerDependenciesJson, consumerCompileDefinitionsArray);

  targetFileJson["CONSUMER_DEPENDENCIES"] = consumerDependenciesJson;
  return targetFileJson;
}

void Target::configure(const Package &package, const PackageVariant &variant, int count) const {
  Json targetFileJson;
  targetFileJson = convertToJson(package, variant, count);
  fs::path targetFileBuildDir = package.packageConfigureDirectory.path / fs::path(std::to_string(count)) / fs::path(configureDirectoryPathRelativeToPV);
  fs::create_directory(targetFileBuildDir);
  fs::path filePath = targetFileBuildDir / fs::path(getFileName());
  std::ofstream(filePath.string()) << targetFileJson.dump(4);
  for (const auto &libDep : libraryDependencies) {
    libDep.library.configure(package, variant, count);
  }
}

Executable::Executable(std::string targetName_) : Target(std::move(targetName_)) {
}
Executable::Executable(std::string targetName_, const fs::path &configureDirectoryPathRelativeToProjectBuildPath)
    : Target(std::move(targetName_), configureDirectoryPathRelativeToProjectBuildPath) {
}

std::string Executable::getFileName() const {
  return targetName + ".executable.hmake";
}

Library::Library(std::string targetName_)
    : libraryType(Project::libraryType), Target(std::move(targetName_)) {
}
Library::Library(std::string targetName_, const fs::path &configureDirectoryPathRelativeToProjectBuildPath)
    : libraryType(Project::libraryType), Target(std::move(targetName_),
                                                configureDirectoryPathRelativeToProjectBuildPath) {
}

std::string Library::getFileName() const {
  if (libraryType == LibraryType::STATIC) {
    return targetName + ".static.hmake";
  }
  return targetName + ".shared.hmake";
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

void to_json(Json &j, const Linker &linker) {
  if (linker.linkerFamily == LinkerFamily::GCC) {
    j["FAMILY"] = "GCC";
  } else if (linker.linkerFamily == LinkerFamily::MSVC) {
    j["FAMILY"] = "MSVC";
  } else {
    j["FAMILY"] = "CLANG";
  }
  j["PATH"] = linker.path.string();
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

void to_json(Json &j, const Version &p) {
  j = std::to_string(p.majorVersion) + "." + std::to_string(p.minorVersion) + "." + std::to_string(p.patchVersion);
}

void to_json(Json &j, const Project &p) {
  j["PROJECT_NAME"] = Project::name;
  j["PROJECT_VERSION"] = Project::version;
  //TODO: Following two are not required.
  j["SOURCE_DIRECTORY"] = Project::sourceDirectory.path.string();
  j["BUILD_DIRECTORY"] = Project::configureDirectory.path.string();
  j["CONFIGURATION"] = Project::projectConfigurationType;
  j["COMPILER"] = Project::compiler;
  j["LINKER"] = Project::linker;
  std::string compilerFlags = Project::flags[Project::compiler.compilerFamily][Project::projectConfigurationType];
  j["COMPILER_FLAGS"] = compilerFlags;
  std::string linkerFlags = Project::flags[Project::linker.linkerFamily][Project::projectConfigurationType];
  j["LINKER_FLAGS"] = linkerFlags;

  JArray exeArray;
  for (const auto &exe : Project::executables) {
    Json exeObject;
    exeObject["NAME"] = exe.targetName;
    exeObject["LOCATION"] = exe.configureDirectory.path.string();
    exeArray.push_back(exeObject);
  }
  jsonAssignSpecialist("EXECUTABLES", j, exeArray);

  JArray libArray;
  for (const auto &lib : Project::libraries) {
    Json libObject;
    libObject["NAME"] = lib.targetName;
    libObject["LOCATION"] = lib.configureDirectory.path.string();
    libArray.push_back(libObject);
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

  JArray compilerArrayJson = cacheJson.at("COMPILER_ARRAY").get<JArray>();
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
    Cache::compilerArray.push_back(compiler);
  }
  Cache::selectedCompilerArrayIndex = cacheJson.at("COMPILER_SELECTED_ARRAY_INDEX").get<int>();

  JArray linkerArrayJson = cacheJson.at("LINKER_ARRAY").get<JArray>();
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
    Cache::linkerArray.push_back(linker);
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
  Project::name = projectName;
  Project::version = projectVersion;
  Project::sourceDirectory = Cache::SOURCE_DIRECTORY;
  Project::configureDirectory = Cache::CONFIGURE_DIRECTORY;
  Project::projectConfigurationType = Cache::projectConfigurationType;
  auto p = Cache::compilerArray;
  Project::compiler = Cache::compilerArray[Cache::selectedCompilerArrayIndex];
  Project::linker = Cache::linkerArray[Cache::selectedLinkerArrayIndex];
  Project::libraryType = Cache::libraryType;
  Project::hasParent = Cache::hasParent;
  Project::parentPath = Cache::parentPath;
  Project::flags[CompilerFamily::GCC][ConfigType::DEBUG] = "-g";
  Project::flags[CompilerFamily::GCC][ConfigType::RELEASE] = "-O3 -DNDEBUG";
}

void initializeCacheAndInitializeProject(int argc, char const **argv, const std::string &projectName, Version projectVersion) {
  initializeCache(argc, argv);
  initializeProject(projectName, projectVersion);
}

void configure() {
  Json json = Project();
  fs::path p = Project::configureDirectory.path;
  std::string fileName = Project::name + ".hmake";
  p /= fileName;
  std::ofstream(p) << json.dump(4);
  for (auto &exe : Project::executables) {
    exe.configure();
  }
  for (auto &lib : Project::libraries) {
    lib.configure();
  }
}

PackageVariant::PackageVariant() {
  configurationType = Project::projectConfigurationType;
  compiler = Project::compiler;
  linker = Project::linker;
  flags = Project::flags;
  libraryType = Project::libraryType;
}

Json PackageVariant::convertToJson(const Directory &packageConfigureDirectory, int count) {
  Json variantFileJson;
  variantFileJson["CONFIGURATION"] = configurationType;
  variantFileJson["COMPILER"] = compiler;
  variantFileJson["LINKER"] = linker;
  //Flags flags = flags;
  std::string compilerFlags = flags[compiler.compilerFamily][configurationType];
  std::string linkerFlags = flags[linker.linkerFamily][configurationType];
  variantFileJson["COMPILER_FLAGS"] = compilerFlags;
  variantFileJson["LINKER_FLAGS"] = linkerFlags;
  variantFileJson["LIBRARY_TYPE"] = libraryType;
  JArray exeArray;
  for (const auto &exe : executables) {

    fs::path exeFilePathPV = packageConfigureDirectory.path / fs::path(std::to_string(count))
        / exe.configureDirectoryPathRelativeToPV;
    Json exeObject;
    //TODO: Maybe Not Required
    exeObject["NAME"] = exe.targetName;
    exeObject["LOCATION"] = exeFilePathPV.string();
    exeArray.push_back(exeObject);
  }
  jsonAssignSpecialist("EXECUTABLES", variantFileJson, exeArray);

  JArray libArray;
  for (const auto &lib : libraries) {
    Json libObject;
    libObject["NAME"] = lib.targetName;
    libObject["LOCATION"] = (packageConfigureDirectory.path / fs::path(std::to_string(count))
                             / lib.configureDirectoryPathRelativeToPV)
                                .string();
    libArray.push_back(libObject);
  }
  jsonAssignSpecialist("LIBRARIES", variantFileJson, libArray);
  return variantFileJson;
}

Package::Package(const fs::path &packageConfigureDirectoryPathRelativeToConfigureDirectory) {
  fs::path directoryPath = Project::configureDirectory.path / packageConfigureDirectoryPathRelativeToConfigureDirectory / "";
  fs::create_directory(directoryPath);
  packageConfigureDirectory = Directory(directoryPath);
}

void Package::configure() {
  checkForSimilarJsonsInPackageVariants();

  Json packageFileJson;
  int count = 0;
  for (auto &i : packageVariants) {
    if (i.json.contains("INDEX")) {
      throw std::runtime_error("Package Variant Json can not have COUNT in it's Json.");
    }
    i.json["INDEX"] = std::to_string(count);
    packageFileJson.emplace_back(i.json);
    ++count;
  }
  fs::path file = packageConfigureDirectory.path / "package.hmake";
  std::ofstream(file) << packageFileJson.dump(4);

  count = 0;
  for (auto &variant : packageVariants) {
    Json variantJsonFile = variant.convertToJson(packageConfigureDirectory, count);
    fs::path variantFileDir = packageConfigureDirectory.path / fs::path(std::to_string(count));
    fs::create_directory(variantFileDir);
    fs::path variantFilePath = variantFileDir / fs::path("variant.hmake");
    std::ofstream(variantFilePath) << variantJsonFile.dump(4);
    for (const auto &exe : variant.executables) {
      exe.configure(*this, variant, count);
    }
    for (const auto &lib : variant.libraries) {
      lib.configure(*this, variant, count);
    }
    ++count;
  }
}

void Package::checkForSimilarJsonsInPackageVariants() {
  //Check that no two JSON'S of packageVariants of package are same
  std::set<nlohmann::json> checkSet;
  for (auto &i : packageVariants) {
    //TODO: Implement this check correctly.
    //This associatedJson is ordered_json, thus two different json's equality test will be false if the order is different even if they have same elements.
    //Thus we first convert it into json normal which is unordered one.
    /* nlohmann::json j = i.json;
    if (auto [pos, ok] = checkSet.emplace(j); !ok) {
      throw std::logic_error("No two json's of packagevariants can be same. Different order with same values does not make two Json's different");
    }*/
  }
}

SubDirectory::SubDirectory(const fs::path &subDirectorySourcePathRelativeToParentSourcePath)
    : SubDirectory(Directory(), Directory()) {
  sourceDirectory = Directory(subDirectorySourcePathRelativeToParentSourcePath);
  auto subDirectoryBuildDirectoryPath =
      Project::configureDirectory.path / subDirectorySourcePathRelativeToParentSourcePath;
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
    cacheFileJson["PARENT_PATH"] = Project::configureDirectory.path.string();
    cacheFileJson["USER_DEFINED"] = cacheVariablesJson;
    std::ofstream(cacheFilePath) << cacheFileJson.dump(4);
    //TODO: Here we will call the compilation command somehow.
  }
}
