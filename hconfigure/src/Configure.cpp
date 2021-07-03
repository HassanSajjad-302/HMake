
#include "Configure.hpp"

#include "fstream"
#include "iostream"
#include "set"

File::File(std::filesystem::__cxx11::path path_) {
  if (path_.is_relative()) {
    path_ = Cache::sourceDirectory.path / path_;
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
    path_ = Cache::sourceDirectory.path / path_;
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

void to_json(Json &j, const CompileDefinition &cd) {
  j["NAME"] = cd.name;
  j["VALUE"] = cd.value;
}

void from_json(const Json &j, CompileDefinition &cd) {
  j.at("NAME").get_to(cd.name);
  j.at("VALUE").get_to(cd.value);
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

static void jsonAssignSpecialist(const std::string &jstr, Json &j, const auto &container) {
  j[jstr] = container;
  /*
  if (container.empty()) {
    return;
  }
  if (container.size() == 1) {
    j[jstr] = *container.begin();
    return;
  }
  j[jstr] = container;*/
}

Target::Target(std::string targetName_, const Variant &variant)
    : targetName(std::move(targetName_)), outputName(targetName) {
  fs::path targetConfigureDirectory = getTargetConfigureDirectoryPath();
  fs::create_directory(targetConfigureDirectory);
  outputDirectory = Directory(targetConfigureDirectory);
  configurationType = variant.configurationType;
  compiler = variant.compiler;
  linker = variant.linker;
  flags = variant.flags;
}

std::string Target::getFileName() const {
  assert(false && "This function should not be called");
  throw std::runtime_error("This function should not be called.");
}

fs::path Target::getTargetConfigureDirectoryPath() const {
  return Cache::configureDirectory.path / targetName;
}

fs::path Target::getTargetVariantDirectoryPath(int variantCount) const {
  return Cache::configureDirectory.path / "Package" / std::to_string(variantCount) / targetName;
}

void Target::assignDifferentVariant(const Variant &variant) {
  configurationType = variant.configurationType;
  compiler = variant.compiler;
  linker = variant.linker;
  flags = variant.flags;
  for (auto &i : libraryDependencies) {
    if (i.ldlt == LDLT::LIBRARY) {
      i.library.assignDifferentVariant(variant);
    }
  }
}

Json Target::convertToJson() const {
  Json targetFileJson;

  std::vector<std::string> sourceFilesArray;
  std::vector<std::string> includeDirectories;
  std::string compilerFlags;
  std::string linkerFlags;
  std::vector<Json> compileDefinitionsArray;
  std::vector<Json> dependenciesArray;

  for (const auto &e : sourceFiles) {
    sourceFilesArray.push_back(e.path.string());
  }
  for (const auto &e : includeDirectoryDependencies) {
    includeDirectories.push_back(e.includeDirectory.path);
  }
  for (const auto &e : compilerFlagsDependencies) {
    compilerFlags.append(" " + e.compilerFlags + " ");
  }
  for (const auto &e : linkerFlagsDependencies) {
    linkerFlags.append(" " + e.linkerFlags + " ");
  }
  for (const auto &e : compileDefinitionDependencies) {
    Json compileDefinitionObject = e.compileDefinition;
    compileDefinitionsArray.push_back(compileDefinitionObject);
  }

  std::vector<const LibraryDependency *> dependencies;
  dependencies = LibraryDependency::getDependencies(*this);
  for (const auto &libraryDependency : dependencies) {
    if (libraryDependency->ldlt == LDLT::LIBRARY) {
      const Library &library = libraryDependency->library;
      for (const auto &e : library.includeDirectoryDependencies) {
        if (e.dependencyType == DependencyType::PUBLIC) {
          includeDirectories.push_back(e.includeDirectory.path);
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
          Json compileDefinitionObject = e.compileDefinition;
          compileDefinitionsArray.push_back(compileDefinitionObject);
        }
      }
      Json libDepObject;
      libDepObject["PREBUILT"] = false;
      libDepObject["PATH"] = (library.getTargetConfigureDirectoryPath()
                              / library.getFileName())
                                 .string();
      dependenciesArray.push_back(libDepObject);
    } else {
      auto populateDependencies = [&](const PLibrary &pLibrary) {
        for (const auto &e : pLibrary.includeDirectoryDependencies) {
          includeDirectories.push_back(e.path);
        }
        compilerFlags.append(" " + pLibrary.compilerFlagsDependencies + " ");
        linkerFlags.append(" " + pLibrary.linkerFlagsDependencies + " ");

        for (const auto &e : pLibrary.compileDefinitionDependencies) {
          Json compileDefinitionObject = e;
          compileDefinitionsArray.push_back(compileDefinitionObject);
        }
        Json libDepObject;
        libDepObject["PREBUILT"] = true;
        libDepObject["PATH"] = pLibrary.libraryPath;
        dependenciesArray.push_back(libDepObject);
      };
      if (libraryDependency->ldlt == LDLT::PLIBRARY) {
        const PLibrary &pLibrary = libraryDependency->pLibrary;
        populateDependencies(pLibrary);
      } else {
        const PLibrary &ppLibrary = libraryDependency->ppLibrary;
        populateDependencies(ppLibrary);
      }
    }
  }

  jsonAssignSpecialist("OUTPUT_NAME", targetFileJson, outputName);
  jsonAssignSpecialist("OUTPUT_DIRECTORY", targetFileJson, outputDirectory.path.string());
  jsonAssignSpecialist("CONFIGURATION", targetFileJson, configurationType);
  jsonAssignSpecialist("COMPILER", targetFileJson, compiler);
  jsonAssignSpecialist("LINKER", targetFileJson, linker);
  jsonAssignSpecialist("COMPILER_FLAGS", targetFileJson, flags[compiler.compilerFamily][configurationType]);
  jsonAssignSpecialist("LINKER_FLAGS", targetFileJson, flags[linker.linkerFamily][configurationType]);
  jsonAssignSpecialist("SOURCE_FILES", targetFileJson, sourceFilesArray);
  jsonAssignSpecialist("LIBRARY_DEPENDENCIES", targetFileJson, dependenciesArray);
  jsonAssignSpecialist("INCLUDE_DIRECTORIES", targetFileJson, includeDirectories);
  jsonAssignSpecialist("COMPILER_TRANSITIVE_FLAGS", targetFileJson, compilerFlags);
  jsonAssignSpecialist("LINKER_TRANSITIVE_FLAGS", targetFileJson, linkerFlags);
  jsonAssignSpecialist("COMPILE_DEFINITIONS", targetFileJson, compileDefinitionsArray);

  targetFileJson["IS_IN_PACKAGE"] = false;
  return targetFileJson;
}

void Target::configure() const {
  Json targetFileJson;
  targetFileJson = convertToJson();
  fs::path targetFilePath = getTargetConfigureDirectoryPath() / getFileName();
  std::ofstream(targetFilePath) << targetFileJson.dump(4);
  for (const auto &libDep : libraryDependencies) {
    if (libDep.ldlt == LDLT::LIBRARY) {
      libDep.library.configure();
    }
  }
}

Json Target::convertToJson(const Package &package, const PackageVariant &variant, int count) const {
  Json targetFileJson;

  std::vector<std::string> sourceFilesArray;
  Json includeDirectoriesArray;
  std::string compilerFlags;
  std::string linkerFlags;
  std::vector<Json> compileDefinitionsArray;
  std::vector<Json> dependenciesArray;

  std::vector<std::string> consumerIncludeDirectories;
  std::string consumerCompilerFlags;
  std::string consumerLinkerFlags;
  std::vector<Json> consumerCompileDefinitionsArray;
  std::vector<Json> consumerDependenciesArray;

  for (const auto &e : sourceFiles) {
    sourceFilesArray.push_back(e.path.string());
  }
  for (const auto &e : includeDirectoryDependencies) {
    Json JIDDObject;
    JIDDObject["PATH"] = e.includeDirectory.path.string();
    if (e.dependencyType == DependencyType::PUBLIC) {
      JIDDObject["COPY"] = true;
      if (e.includeDirectory.isCommon) {
        std::cout << "IsCommon True For " << e.includeDirectory.path << std::endl;
        std::cout << "Common Directory Number Is " << e.includeDirectory.commonDirectoryNumber << std::endl;
      } else {
        std::cout << "IsCommon False For " << e.includeDirectory.path << std::endl;
      }
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
    Json compileDefinitionObject = e.compileDefinition;
    compileDefinitionsArray.push_back(compileDefinitionObject);
    if (e.dependencyType == DependencyType::PUBLIC) {
      consumerCompileDefinitionsArray.push_back(compileDefinitionObject);
    }
  }

  std::vector<const LibraryDependency *> dependencies;
  dependencies = LibraryDependency::getDependencies(*this);
  for (const auto &libraryDependency : dependencies) {
    if (libraryDependency->ldlt == LDLT::LIBRARY) {
      const Library &library = libraryDependency->library;
      for (const auto &e : library.includeDirectoryDependencies) {
        if (e.dependencyType == DependencyType::PUBLIC) {
          Json JIDDObject;
          JIDDObject["PATH"] = e.includeDirectory.path.string();
          JIDDObject["COPY"] = false;
          includeDirectoriesArray.push_back(JIDDObject);
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
          Json compileDefinitionObject = e.compileDefinition;
          compileDefinitionsArray.push_back(compileDefinitionObject);
        }
      }

      Json libDepObject;
      libDepObject["PREBUILT"] = false;
      Json consumeLibDepOject;
      consumeLibDepOject["IMPORTED"] = false;
      consumeLibDepOject["NAME"] = library.targetName;
      consumerDependenciesArray.push_back(consumeLibDepOject);
      libDepObject["PATH"] = (library.getTargetVariantDirectoryPath(count) / library.getFileName()).string();
      dependenciesArray.push_back(libDepObject);
    } else {
      Json libDepObject;
      libDepObject["PREBUILT"] = true;
      Json consumeLibDepOject;
      if (libraryDependency->ldlt == LDLT::PLIBRARY) {
        const auto &pLibrary = libraryDependency->pLibrary;
        for (const auto &e : pLibrary.includeDirectoryDependencies) {
          Json JIDDObject;
          JIDDObject["PATH"] = e.path;
          JIDDObject["COPY"] = false;

          includeDirectoriesArray.push_back(JIDDObject);
        }
        compilerFlags.append(" " + pLibrary.compilerFlagsDependencies + " ");
        linkerFlags.append(" " + pLibrary.linkerFlagsDependencies + " ");

        for (const auto &e : pLibrary.compileDefinitionDependencies) {
          Json compileDefinitionObject = e;
          compileDefinitionsArray.push_back(compileDefinitionObject);
        }
        libDepObject["PATH"] = pLibrary.libraryPath;
        libDepObject["IMPORTED"] = false;
        libDepObject["HMAKE_FILE_PATH"] = (pLibrary.getTargetVariantDirectoryPath(count) / pLibrary.getFileName()).string();
        consumeLibDepOject["IMPORTED"] = false;
        consumeLibDepOject["NAME"] = pLibrary.libraryName;
      } else {
        const auto &ppLibrary = libraryDependency->ppLibrary;
        for (const auto &e : ppLibrary.includeDirectoryDependencies) {
          Json JIDDObject;
          JIDDObject["PATH"] = e.path;
          JIDDObject["COPY"] = false;
          includeDirectoriesArray.push_back(JIDDObject);
        }
        compilerFlags.append(" " + ppLibrary.compilerFlagsDependencies + " ");
        linkerFlags.append(" " + ppLibrary.linkerFlagsDependencies + " ");

        for (const auto &e : ppLibrary.compileDefinitionDependencies) {
          Json compileDefinitionObject = e;
          compileDefinitionsArray.push_back(compileDefinitionObject);
        }
        libDepObject["PATH"] = ppLibrary.libraryPath;
        libDepObject["IMPORTED"] = ppLibrary.imported;
        if (!ppLibrary.imported) {
          libDepObject["HMAKE_FILE_PATH"] = (ppLibrary.getTargetVariantDirectoryPath(count) / ppLibrary.getFileName()).string();
        }

        consumeLibDepOject["IMPORTED"] = ppLibrary.imported;
        consumeLibDepOject["NAME"] = ppLibrary.libraryName;
        if (ppLibrary.imported) {
          consumeLibDepOject["PACKAGE_NAME"] = ppLibrary.packageName;
          consumeLibDepOject["PACKAGE_VERSION"] = ppLibrary.packageVersion;
          consumeLibDepOject["PACKAGE_PATH"] = ppLibrary.packagePath;
          consumeLibDepOject["USE_INDEX"] = ppLibrary.useIndex;
          consumeLibDepOject["INDEX"] = ppLibrary.index;
          consumeLibDepOject["PACKAGE_VARIANT_JSON"] = ppLibrary.packageVariantJson;
        }
      }
      consumerDependenciesArray.push_back(consumeLibDepOject);
      dependenciesArray.push_back(libDepObject);
    }
  }

  jsonAssignSpecialist("OUTPUT_NAME", targetFileJson, outputName);
  jsonAssignSpecialist("OUTPUT_DIRECTORY", targetFileJson, getTargetVariantDirectoryPath(count));
  jsonAssignSpecialist("CONFIGURATION", targetFileJson, configurationType);
  jsonAssignSpecialist("COMPILER", targetFileJson, compiler);
  jsonAssignSpecialist("LINKER", targetFileJson, linker);
  jsonAssignSpecialist("COMPILER_FLAGS", targetFileJson, flags[compiler.compilerFamily][configurationType]);
  jsonAssignSpecialist("LINKER_FLAGS", targetFileJson, flags[linker.linkerFamily][configurationType]);
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
  targetFileJson["IS_IN_PACKAGE"] = true;
  targetFileJson["PACKAGE_COPY"] = Cache::copyPackage;
  if (Cache::copyPackage) {
    targetFileJson["PACKAGE_NAME"] = package.name;
    targetFileJson["PACKAGE_VERSION"] = package.version;
    targetFileJson["PACKAGE_COPY_PATH"] = Cache::packageCopyPath;
    targetFileJson["PACKAGE_VARIANT_INDEX"] = count;
  }
  return targetFileJson;
}

void Target::configure(const Package &package, const PackageVariant &variant, int count) const {
  Json targetFileJson;
  targetFileJson = convertToJson(package, variant, count);
  fs::path targetFileBuildDir = getTargetVariantDirectoryPath(count);
  fs::create_directory(targetFileBuildDir);
  fs::path filePath = targetFileBuildDir / getFileName();
  std::ofstream(filePath.string()) << targetFileJson.dump(4);
  for (const auto &libDep : libraryDependencies) {
    if (libDep.ldlt == LDLT::LIBRARY) {
      libDep.library.configure(package, variant, count);
    } else if (libDep.ldlt == LDLT::PLIBRARY) {
      libDep.pLibrary.configure(package, variant, count);
    } else if (!libDep.ppLibrary.imported) {
      libDep.ppLibrary.configure(package, variant, count);
    }
  }
}

Executable::Executable(std::string targetName_, const Variant &variant) : Target(std::move(targetName_), variant) {
}

std::string Executable::getFileName() const {
  return targetName + ".executable.hmake";
}

void Executable::assignDifferentVariant(const Variant &variant) {
  Target::assignDifferentVariant(variant);
}

Library::Library(std::string targetName_, const Variant &variant)
    : libraryType(variant.libraryType), Target(std::move(targetName_), variant) {
}

std::string Library::getFileName() const {
  if (libraryType == LibraryType::STATIC) {
    return targetName + ".static.hmake";
  }
  return targetName + ".shared.hmake";
}

void Library::assignDifferentVariant(const Variant &variant) {
  Target::assignDifferentVariant(variant);
  libraryType = variant.libraryType;
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

Flags &Flags::operator[](CompilerFamily compilerFamily) const {
  if (compileHelper || linkHelper || configHelper) {
    throw std::logic_error("Wrong Usage Of Flag Class.");
  }
  compileHelper = true;
  compilerCurrent = compilerFamily;
  return const_cast<Flags &>(*this);
}
Flags &Flags::operator[](LinkerFamily linkerFamily) const {
  if (compileHelper || linkHelper || configHelper) {
    throw std::logic_error("Wrong Usage Of Flag Class.");
  }
  linkHelper = true;
  linkerCurrent = linkerFamily;
  return const_cast<Flags &>(*this);
}
Flags &Flags::operator[](ConfigType configType) const {
  if (!compileHelper && !linkHelper) {
    throw std::logic_error("Wrong Usage Of Flag Class. First use operator[] with COMPILER_FAMILY or LINKER_FAMILY");
  }
  configHelper = true;
  configCurrent = configType;
  return const_cast<Flags &>(*this);
}

void Flags::operator=(const std::string &flags1) {
  if ((!compileHelper && !linkHelper) && !configHelper) {
    throw std::logic_error("Wrong Usage Of Flag Class.");
  }
  configHelper = false;
  if (compileHelper) {
    auto t = std::make_tuple(compilerCurrent, configCurrent);
    if (auto [pos, ok] = compilerFlags.emplace(t, flags1); !ok) {
      std::cout << "Rewriting the flags in compilerFlags for this configuration";
      compilerFlags[t] = flags1;
    }
    compileHelper = false;
  } else {
    auto t = std::make_tuple(linkerCurrent, configCurrent);
    if (auto [pos, ok] = linkerFlags.emplace(t, flags1); !ok) {
      std::cout << "Rewriting the flags in linkerFlags for this configuration";
      linkerFlags[t] = flags1;
    }
    linkHelper = false;
  }
}

Flags Flags::defaultFlags() {
  Flags flags1;
  flags1[CompilerFamily::GCC][ConfigType::DEBUG] = "-g";
  flags1[CompilerFamily::GCC][ConfigType::RELEASE] = "-O3 -DNDEBUG";
  return flags1;
}

Flags::operator std::string() const {
  if ((!compileHelper && !linkHelper) && !configHelper) {
    throw std::logic_error("Wrong Usage Of Flag Class.");
  }
  configHelper = false;
  if (compileHelper) {
    auto t = std::make_tuple(compilerCurrent, configCurrent);
    compileHelper = false;
    if (compilerFlags.find(t) == compilerFlags.end() || compilerFlags.at(t).empty()) {
      std::cout << "No Compiler Flags Defined For This Configuration." << std::endl;
      return "";
    }
    return compilerFlags.at(t);
  } else {
    auto t = std::make_tuple(linkerCurrent, configCurrent);
    linkHelper = false;
    if (linkerFlags.find(t) == linkerFlags.end() || linkerFlags.at(t).empty()) {
      std::cout << "No Linker Flags Defined For This Configuration." << std::endl;
      return "";
    }
    return linkerFlags.at(t);
  }
}

void Cache::initializeCache() {
  fs::path filePath = fs::current_path() / "cache.hmake";
  if (!exists(filePath) || !is_regular_file(filePath)) {
    throw std::runtime_error(filePath.string() + " does not exists or is not a regular file");
  }
  Json cacheFileJson;
  std::ifstream(filePath) >> cacheFileJson;

  Cache::sourceDirectory = Directory(cacheFileJson.at("SOURCE_DIRECTORY").get<std::string>());
  Cache::configureDirectory = Directory(fs::current_path());

  Cache::copyPackage = cacheFileJson.at("PACKAGE_COPY").get<bool>();
  if (copyPackage) {
    packageCopyPath = cacheFileJson.at("PACKAGE_COPY_PATH").get<std::string>();
  }
  std::string configTypeString = cacheFileJson.at("CONFIGURATION").get<std::string>();
  ConfigType configType;
  if (configTypeString == "DEBUG") {
    configType = ConfigType::DEBUG;
  } else if (configTypeString == "RELEASE") {
    configType = ConfigType::RELEASE;
  } else {
    throw std::runtime_error("Unknown CONFIG_TYPE " + configTypeString);
  }
  Cache::projectConfigurationType = configType;

  Json compilerArrayJson = cacheFileJson.at("COMPILER_ARRAY").get<Json>();
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
  Cache::selectedCompilerArrayIndex = cacheFileJson.at("COMPILER_SELECTED_ARRAY_INDEX").get<int>();

  Json linkerArrayJson = cacheFileJson.at("LINKER_ARRAY").get<Json>();
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
  Cache::selectedLinkerArrayIndex = cacheFileJson.at("COMPILER_SELECTED_ARRAY_INDEX").get<int>();

  std::string libraryTypeString = cacheFileJson.at("LIBRARY_TYPE").get<std::string>();
  LibraryType type;
  if (libraryTypeString == "STATIC") {
    type = LibraryType::STATIC;
  } else if (libraryTypeString == "SHARED") {
    type = LibraryType::SHARED;
  } else {
    throw std::runtime_error("Unknown LIBRARY_TYPE " + libraryTypeString);
  }
  Cache::libraryType = type;
  Cache::cacheVariables = cacheFileJson.at("CACHE_VARIABLES").get<Json>();
}

void Cache::registerCacheVariables() {
  fs::path filePath = fs::current_path() / "cache.hmake";
  Json cacheFileJson;
  std::ifstream(filePath) >> cacheFileJson;
  cacheFileJson["CACHE_VARIABLES"] = Cache::cacheVariables;
  std::ofstream(filePath) << cacheFileJson.dump(2);
}

void to_json(Json &j, const Version &p) {
  j = std::to_string(p.majorVersion) + "." + std::to_string(p.minorVersion) + "." + std::to_string(p.patchVersion);
}

void from_json(const Json &j, Version &v) {
  std::string jString = j;
  char delim = '.';
  std::stringstream ss(jString);
  std::string item;
  int count = 0;
  while (getline(ss, item, delim)) {
    if (count == 0) {
      v.majorVersion = std::stoi(item);
    } else if (count == 1) {
      v.minorVersion = std::stoi(item);
    } else {
      v.patchVersion = std::stoi(item);
    }
    ++count;
  }
}

ProjectVariant::ProjectVariant() {
  configureDirectory = Cache::configureDirectory;
  configurationType = Cache::projectConfigurationType;
  compiler = Cache::compilerArray[Cache::selectedCompilerArrayIndex];
  linker = Cache::linkerArray[Cache::selectedLinkerArrayIndex];
  libraryType = Cache::libraryType;
  flags = ::flags;
}

Json ProjectVariant::convertToJson() {
  Json projectJson;
  projectJson["CONFIGURATION"] = configurationType;
  projectJson["COMPILER"] = compiler;
  projectJson["LINKER"] = linker;
  std::string compilerFlags = flags[compiler.compilerFamily][configurationType];
  projectJson["COMPILER_FLAGS"] = compilerFlags;
  std::string linkerFlags = flags[linker.linkerFamily][configurationType];
  projectJson["LINKER_FLAGS"] = linkerFlags;
  projectJson["LIBRARY_TYPE"] = libraryType;

  std::vector<Json> targetArray;
  for (const auto &exe : executables) {
    std::string path = (exe.getTargetConfigureDirectoryPath() / exe.getFileName()).string();
    targetArray.push_back(path);
  }
  for (const auto &lib : libraries) {
    std::string path = (lib.getTargetConfigureDirectoryPath() / lib.getFileName()).string();
    targetArray.push_back(path);
  }
  jsonAssignSpecialist("TARGETS", projectJson, targetArray);
  return projectJson;
}

void ProjectVariant::configure() {
  Json json = convertToJson();
  fs::path projectFilePath = configureDirectory.path / "projectVariant.hmake";
  std::ofstream(projectFilePath) << json.dump(4);
  for (auto &exe : executables) {
    exe.configure();
  }
  for (auto &lib : libraries) {
    lib.configure();
  }
}

PackageVariant::PackageVariant() {
  configurationType = Cache::projectConfigurationType;
  compiler = Cache::compilerArray[Cache::selectedCompilerArrayIndex];
  linker = Cache::linkerArray[Cache::selectedLinkerArrayIndex];
  flags = ::flags;
  libraryType = Cache::libraryType;
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
  std::vector<std::string> targets;
  for (const auto &exe : executables) {
    std::string path = (exe.getTargetVariantDirectoryPath(count) / exe.getFileName()).string();
    targets.push_back(path);
  }
  for (const auto &lib : libraries) {
    std::string path = (lib.getTargetVariantDirectoryPath(count) / lib.getFileName()).string();
    targets.push_back(path);
  }
  jsonAssignSpecialist("TARGETS", variantFileJson, targets);
  return variantFileJson;
}

Package::Package(std::string name_) : name(std::move(name_)), version{0, 0, 0} {
}

void Package::configureCommonAmongVariants() {
  struct NonCommonIncludeDir {
    const Directory *directory;
    bool isCommon = false;
    int variantIndex;
    int indexInPackageCommonTargetsVector;
  };
  struct CommonIncludeDir {
    std::vector<int> variantsIndices;
    std::vector<Directory *> directories;
  };
  std::vector<NonCommonIncludeDir> packageNonCommonIncludeDirs;
  std::vector<CommonIncludeDir> packageCommonIncludeDirs;

  auto isDirectoryCommonOrNonCommon = [&](Directory *includeDirectory, int index) {
    bool matched = false;
    for (auto &i : packageNonCommonIncludeDirs) {
      if (i.directory->path == includeDirectory->path) {
        if (i.isCommon) {
          packageCommonIncludeDirs[i.indexInPackageCommonTargetsVector].variantsIndices.push_back(index);
        } else {
          CommonIncludeDir includeDir;
          includeDir.directories.push_back(const_cast<Directory *>(i.directory));
          includeDir.directories.push_back(includeDirectory);
          includeDir.variantsIndices.push_back(i.variantIndex);
          includeDir.variantsIndices.push_back(index);
          packageCommonIncludeDirs.push_back(includeDir);
          i.isCommon = true;
          i.indexInPackageCommonTargetsVector = packageCommonIncludeDirs.size() - 1;
        }
        matched = true;
        break;
      }
    }
    if (!matched) {
      NonCommonIncludeDir includeDir;
      includeDir.directory = includeDirectory;
      includeDir.variantIndex = index;
      packageNonCommonIncludeDirs.push_back(includeDir);
    }
  };

  auto assignPackageCommonAndNonCommonIncludeDirsForSingleTarget = [&](Target *target, int index) {
    for (auto &idd : target->includeDirectoryDependencies) {
      if (idd.dependencyType == DependencyType::PUBLIC) {
        isDirectoryCommonOrNonCommon(&(idd.includeDirectory), index);
      }
    }
  };

  auto assignPackageCommonAndNonCommonIncludeDirsForSinglePLibrary = [&](PLibrary *pLibrary, int index) {
    for (auto &id : pLibrary->includeDirectoryDependencies) {
      isDirectoryCommonOrNonCommon(&id, index);
    }
  };

  auto assignPackageCommonAndNonCommonIncludeDirs = [&](const Target *target, int index) {
    assignPackageCommonAndNonCommonIncludeDirsForSingleTarget(const_cast<Target *>(target), index);
    auto dependencies = LibraryDependency::getDependencies(*target);
    for (const auto &libraryDependency : dependencies) {
      if (libraryDependency->ldlt == LDLT::LIBRARY) {
        auto &lib = const_cast<Library &>(libraryDependency->library);
        assignPackageCommonAndNonCommonIncludeDirsForSingleTarget(&lib, index);
      } else {
        auto &pLib = const_cast<PLibrary &>(libraryDependency->pLibrary);
        assignPackageCommonAndNonCommonIncludeDirsForSinglePLibrary(&pLib, index);
      }
    }
  };

  int count = 0;
  for (auto &variant : packageVariants) {
    for (const auto &exe : variant.executables) {
      assignPackageCommonAndNonCommonIncludeDirs(&exe, count);
    }
    for (const auto &lib : variant.libraries) {
      assignPackageCommonAndNonCommonIncludeDirs(&lib, count);
    }
    ++count;
  }

  count = 0;
  for (auto &i : packageCommonIncludeDirs) {
    for (auto j : i.directories) {
      j->isCommon = true;
      j->commonDirectoryNumber = count;
    }
    ++count;
  }
}

void Package::configure() {
  checkForSimilarJsonsInPackageVariants();

  Json packageFileJson;
  Json packageVariantJson;
  int count = 0;
  for (auto &i : packageVariants) {
    if (i.json.contains("INDEX")) {
      throw std::runtime_error("Package Variant Json can not have COUNT in it's Json.");
    }
    i.json["INDEX"] = std::to_string(count);
    packageVariantJson.emplace_back(i.json);
    ++count;
  }
  packageFileJson["NAME"] = name;
  packageFileJson["VERSION"] = version;
  packageFileJson["PACKAGE_COPY"] = Cache::copyPackage;
  packageFileJson["PACKAGE_COPY_PATH"] = Cache::packageCopyPath;
  packageFileJson["VARIANTS"] = packageVariantJson;
  fs::path packageDirectorypath = Cache::configureDirectory.path / "Package";
  fs::create_directory(packageDirectorypath);
  Directory packageDirectory(packageDirectorypath);
  fs::path file = packageDirectory.path / "package.hmake";
  std::ofstream(file) << packageFileJson.dump(4);

  configureCommonAmongVariants();
  count = 0;
  for (auto &variant : packageVariants) {
    Json variantJsonFile = variant.convertToJson(packageDirectory, count);
    fs::path variantFileDir = packageDirectory.path / std::to_string(count);
    fs::create_directory(variantFileDir);
    fs::path variantFilePath = variantFileDir / "packageVariant.hmake";
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

PLibrary::PLibrary(fs::path libraryPath_, LibraryType libraryType_)
    : libraryType(libraryType_) {
  if (libraryPath_.is_relative()) {
    libraryPath = Cache::sourceDirectory.path / libraryPath_;
  }
  if (libraryType == LibraryType::STATIC) {
    libraryName = libraryPath.filename();
    libraryName.erase(0, 3);
    libraryName.erase(libraryName.find('.'), 2);
  }
}

fs::path PLibrary::getTargetVariantDirectoryPath(int variantCount) const {
  return Cache::configureDirectory.path / "Package" / std::to_string(variantCount) / libraryName;
}

std::string PLibrary::getFileName() const {
  if (libraryType == LibraryType::STATIC) {
    return libraryName + ".static.hmake";
  }
  return libraryName + ".shared.hmake";
}

Json PLibrary::convertToJson(const Package &package, const PackageVariant &variant, int count) const {

  Json targetFileJson;

  Json includeDirectoriesArray;
  std::vector<Json> dependenciesArray;

  std::vector<std::string> consumerIncludeDirectories;
  std::string consumerCompilerFlags;
  std::string consumerLinkerFlags;
  std::vector<Json> consumerCompileDefinitionsArray;
  std::vector<Json> consumerDependenciesArray;

  for (const auto &e : includeDirectoryDependencies) {
    Json JIDDObject;
    JIDDObject["PATH"] = e.path.string();
    JIDDObject["COPY"] = true;
    if (e.isCommon) {
      std::cout << "IsCommon True For " << e.path << std::endl;
      std::cout << "Common Directory Number Is " << e.commonDirectoryNumber << std::endl;
    } else {
      std::cout << "IsCommon False For " << e.path << std::endl;
    }
    consumerIncludeDirectories.emplace_back("include/");
    includeDirectoriesArray.push_back(JIDDObject);
  }

  consumerCompilerFlags.append(" " + compilerFlagsDependencies + " ");
  consumerLinkerFlags.append(" " + linkerFlagsDependencies + " ");

  for (const auto &e : compileDefinitionDependencies) {
    Json compileDefinitionObject = e;
    consumerCompileDefinitionsArray.push_back(compileDefinitionObject);
  }

  std::vector<const LibraryDependency *> dependencies;
  dependencies = LibraryDependency::getDependencies(*this);
  for (const auto &libraryDependency : dependencies) {
    if (libraryDependency->ldlt == LDLT::LIBRARY) {
      const Library &library = libraryDependency->library;
      for (const auto &e : library.includeDirectoryDependencies) {
        if (e.dependencyType == DependencyType::PUBLIC) {
          Json JIDDObject;
          JIDDObject["PATH"] = e.includeDirectory.path.string();
          JIDDObject["COPY"] = false;
          includeDirectoriesArray.push_back(JIDDObject);
        }
      }

      Json libDepObject;
      libDepObject["PREBUILT"] = false;
      Json consumeLibDepOject;
      consumeLibDepOject["IMPORTED"] = false;
      consumeLibDepOject["NAME"] = library.targetName;
      consumerDependenciesArray.push_back(consumeLibDepOject);
      libDepObject["PATH"] = (library.getTargetVariantDirectoryPath(count) / library.getFileName()).string();
      dependenciesArray.push_back(libDepObject);
    } else {
      Json libDepObject;
      libDepObject["PREBUILT"] = true;
      Json consumeLibDepOject;
      if (libraryDependency->ldlt == LDLT::PLIBRARY) {
        const auto &pLibrary = libraryDependency->pLibrary;
        for (const auto &e : pLibrary.includeDirectoryDependencies) {
          Json JIDDObject;
          JIDDObject["PATH"] = e.path;
          JIDDObject["COPY"] = false;

          includeDirectoriesArray.push_back(JIDDObject);
        }

        libDepObject["PATH"] = libraryPath;
        libDepObject["IMPORTED"] = false;
        libDepObject["HMAKE_FILE_PATH"] = (pLibrary.getTargetVariantDirectoryPath(count) / pLibrary.getFileName()).string();
        consumeLibDepOject["IMPORTED"] = false;
        consumeLibDepOject["NAME"] = pLibrary.libraryName;
      } else {
        const auto &ppLibrary = libraryDependency->ppLibrary;
        for (const auto &e : ppLibrary.includeDirectoryDependencies) {
          Json JIDDObject;
          JIDDObject["PATH"] = e.path;
          JIDDObject["COPY"] = false;
          includeDirectoriesArray.push_back(JIDDObject);
        }

        libDepObject["PATH"] = ppLibrary.libraryPath;
        libDepObject["IMPORTED"] = ppLibrary.imported;
        if (!ppLibrary.imported) {
          libDepObject["HMAKE_FILE_PATH"] = (ppLibrary.getTargetVariantDirectoryPath(count) / ppLibrary.getFileName()).string();
        }

        consumeLibDepOject["IMPORTED"] = ppLibrary.imported;
        consumeLibDepOject["NAME"] = ppLibrary.libraryName;
        if (ppLibrary.imported) {
          consumeLibDepOject["PACKAGE_NAME"] = ppLibrary.packageName;
          consumeLibDepOject["PACKAGE_VERSION"] = ppLibrary.packageVersion;
          consumeLibDepOject["PACKAGE_PATH"] = ppLibrary.packagePath;
          consumeLibDepOject["USE_INDEX"] = ppLibrary.useIndex;
          consumeLibDepOject["INDEX"] = ppLibrary.index;
          consumeLibDepOject["PACKAGE_VARIANT_JSON"] = ppLibrary.packageVariantJson;
        }
      }
      consumerDependenciesArray.push_back(consumeLibDepOject);
      dependenciesArray.push_back(libDepObject);
    }
  }

  jsonAssignSpecialist("LIBRARY_DEPENDENCIES", targetFileJson, dependenciesArray);
  jsonAssignSpecialist("INCLUDE_DIRECTORIES", targetFileJson, includeDirectoriesArray);

  Json consumerDependenciesJson;
  jsonAssignSpecialist("LIBRARY_DEPENDENCIES", consumerDependenciesJson, consumerDependenciesArray);
  jsonAssignSpecialist("INCLUDE_DIRECTORIES", consumerDependenciesJson, consumerIncludeDirectories);
  jsonAssignSpecialist("COMPILER_TRANSITIVE_FLAGS", consumerDependenciesJson, consumerCompilerFlags);
  jsonAssignSpecialist("LINKER_TRANSITIVE_FLAGS", consumerDependenciesJson, consumerLinkerFlags);
  jsonAssignSpecialist("COMPILE_DEFINITIONS", consumerDependenciesJson, consumerCompileDefinitionsArray);

  targetFileJson["CONSUMER_DEPENDENCIES"] = consumerDependenciesJson;
  targetFileJson["IS_IN_PACKAGE"] = true;
  targetFileJson["PACKAGE_COPY"] = Cache::copyPackage;
  if (Cache::copyPackage) {
    targetFileJson["PACKAGE_NAME"] = package.name;
    targetFileJson["PACKAGE_VERSION"] = package.version;
    targetFileJson["PACKAGE_COPY_PATH"] = Cache::packageCopyPath;
    targetFileJson["PACKAGE_VARIANT_INDEX"] = count;
  }
  return targetFileJson;
}

void PLibrary::configure(const Package &package, const PackageVariant &variant, int count) const {
  Json targetFileJson;
  targetFileJson = convertToJson(package, variant, count);
  fs::path targetFileBuildDir = Cache::configureDirectory.path / "Package" / std::to_string(count) / libraryName;
  fs::create_directory(targetFileBuildDir);
  std::string fileExt;
  if (libraryType == LibraryType::STATIC) {
    fileExt = ".static.hmake";
  } else {
    fileExt = ".shared.hmake";
  }
  fs::path filePath = targetFileBuildDir / (libraryName + fileExt);
  std::ofstream(filePath.string()) << targetFileJson.dump(4);
  for (const auto &libDep : libraryDependencies) {
    if (libDep.ldlt == LDLT::LIBRARY) {
      libDep.library.configure(package, variant, count);
    } else if (libDep.ldlt == LDLT::PLIBRARY) {
      libDep.pLibrary.configure(package, variant, count);
    } else if (!libDep.ppLibrary.imported) {
      libDep.ppLibrary.configure(package, variant, count);
    }
  }
}

LibraryDependency::LibraryDependency(Library library_, DependencyType dependencyType_)
    : library(std::move(library_)), dependencyType(dependencyType_), ldlt(LDLT::LIBRARY) {
}

LibraryDependency::LibraryDependency(PLibrary pLibrary_, DependencyType dependencyType_)
    : pLibrary(std::move(pLibrary_)), dependencyType(dependencyType_), ldlt(LDLT::PLIBRARY) {
}

LibraryDependency::LibraryDependency(PPLibrary ppLibrary_, DependencyType dependencyType_)
    : ppLibrary(std::move(ppLibrary_)), dependencyType(dependencyType_), ldlt(LDLT::PPLIBRARY) {
}

PPLibrary::PPLibrary(std::string libraryName_, const CPackage &cPackage, const CPVariant &cpVariant) {

  libraryName = std::move(libraryName_);
  fs::path libraryDirectoryPath;
  bool found = false;
  for (auto &i : fs::directory_iterator(cpVariant.variantPath)) {
    if (i.path().filename() == libraryName && i.is_directory()) {
      libraryDirectoryPath = i;
      found = true;
      break;
    }
  }
  if (!found) {
    throw std::runtime_error("Library " + libraryName + " Not Present In Package Variant");
  }

  fs::path staticLibPath = libraryDirectoryPath / (libraryName + ".static.hmake");
  fs::path sharedLibPath = libraryDirectoryPath / (libraryName + ".shared.hmake");
  fs::path libraryFilePath;
  if (fs::exists(staticLibPath)) {
    libraryFilePath = staticLibPath;
    libraryType = LibraryType::STATIC;
  } else if (fs::exists(sharedLibPath)) {
    libraryFilePath = sharedLibPath;
    libraryType = LibraryType::SHARED;
  } else {
    throw std::runtime_error("None of " + staticLibPath.string() + " Or " + sharedLibPath.string() + " Exists.");
  }

  Json libraryFileJson;
  std::ifstream(libraryFilePath) >> libraryFileJson;

  Json libDependencies = libraryFileJson.at("LIBRARY_DEPENDENCIES").get<Json>();
  for (auto &i : libDependencies) {
    bool isImported = i.at("IMPORTED").get<bool>();
    if (!isImported) {
      std::string dependencyLibraryName = i.at("NAME").get<std::string>();
      PPLibrary ppLibrary(dependencyLibraryName, cPackage, cpVariant);
      libraryDependencies.emplace_back(ppLibrary, DependencyType::PUBLIC);
    } else {
      CPackage pack(i.at("PACKAGE_PATH").get<std::string>());
      std::string libName = i.at("NAME").get<std::string>();
      if (i.at("USE_INDEX").get<bool>()) {
        PPLibrary ppLibrary(libName, pack, pack.getVariant(i.at("INDEX").get<int>()));
        libraryDependencies.emplace_back(ppLibrary, DependencyType::PUBLIC);
      } else {
        PPLibrary ppLibrary(libName, pack,
                            pack.getVariant(i.at("PACKAGE_VARIANT_JSON").get<Json>()));
        libraryDependencies.emplace_back(ppLibrary, DependencyType::PUBLIC);
      }
    }
  }
  std::vector<std::string> includeDirs = libraryFileJson.at("INCLUDE_DIRECTORIES").get<std::vector<std::string>>();
  for (auto &i : includeDirs) {
    Directory dir(libraryDirectoryPath / i);
    includeDirectoryDependencies.push_back(dir);
  }
  compilerFlagsDependencies = libraryFileJson.at("COMPILER_TRANSITIVE_FLAGS").get<std::string>();
  linkerFlagsDependencies = libraryFileJson.at("LINKER_TRANSITIVE_FLAGS").get<std::string>();
  if (!libraryFileJson.at("COMPILE_DEFINITIONS").empty()) {
    compileDefinitionDependencies =
        libraryFileJson.at("COMPILE_DEFINITIONS").get<std::vector<CompileDefinition>>();
  }
  libraryPath = libraryDirectoryPath / fs::path("lib" + libraryName + ".a");
  packageName = cPackage.name;
  packageVersion = cPackage.version;
  packagePath = cPackage.path;
  packageVariantJson = cpVariant.variantJson;
  index = cpVariant.index;
}

CPVariant::CPVariant(fs::path variantPath_, Json variantJson_, int index_)
    : variantPath(std::move(variantPath_)), variantJson(std::move(variantJson_)), index(index_) {
}

CPackage::CPackage(fs::path packagePath_) : path(std::move(packagePath_)) {
  if (path.is_relative()) {
    path = Cache::sourceDirectory.path / path;
  }
  path = path.lexically_normal();
  fs::path packageFilePath = path / "cpackage.hmake";
  Json packageFileJson;
  std::ifstream(packageFilePath) >> packageFileJson;
  name = packageFileJson["NAME"];
  version = packageFileJson["VERSION"];
  variantsJson = packageFileJson["VARIANTS"];
}

CPVariant CPackage::getVariant(const Json &variantJson_) {
  int numberOfMatches = 0;
  int matchIndex;
  for (int i = 0; i < variantsJson.size(); ++i) {
    bool matched = true;
    for (const auto &keyValuePair : variantJson_.items()) {
      if (!variantsJson[i].contains(keyValuePair.key()) || variantsJson[i][keyValuePair.key()] != keyValuePair.value()) {
        matched = false;
        break;
      }
    }
    if (matched) {
      ++numberOfMatches;
      matchIndex = i;
    }
  }
  if (numberOfMatches == 1) {
    std::string index = variantsJson[matchIndex].at("INDEX").get<std::string>();
    return CPVariant(path / index, variantJson_, std::stoi(index));
  } else if (numberOfMatches == 0) {
    throw std::runtime_error("No Json in package " + path.string() + " matches \n" + variantJson_.dump(2));
  } else if (numberOfMatches > 1) {
    throw std::runtime_error("More than 1 Jsons in package " + path.string() + " matches \n" + variantJson_.dump(2));
  }
}

CPVariant CPackage::getVariant(const int index) {
  int numberOfMatches = 0;
  int matchIndex;
  for (const auto &i : variantsJson) {
    if (i.at("INDEX").get<std::string>() == std::to_string(index)) {
      return CPVariant(path / std::to_string(index), i, index);
    }
  }
  throw std::runtime_error("No Json in package " + path.string() + " has index " + std::to_string(index));
}