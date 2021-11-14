
#include "Configure.hpp"

#include "fstream"
#include "iostream"
#include "set"

File::File(fs::path path_) {
  if (path_.is_relative()) {
    path_ = Cache::sourceDirectory.path / path_;
  }
  if (fs::directory_entry(path_).status().type() == fs::file_type::regular) {
    path = path_.lexically_normal();
    return;
  }
  throw std::runtime_error(path_.string() + " Is Not a Regular File");
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

void to_json(Json &j, const DependencyType &p) {
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

void from_json(const Json &json, CompilerFamily &compilerFamily) {
  if (json == "GCC") {
    compilerFamily = CompilerFamily::GCC;
  } else if (json == "MSVC") {
    compilerFamily = CompilerFamily::MSVC;
  } else if (json == "CLANG") {
    compilerFamily = CompilerFamily::CLANG;
  }
}

void from_json(const Json &json, LinkerFamily &linkerFamily) {
  if (json == "GCC") {
    linkerFamily = LinkerFamily::GCC;
  } else if (json == "MSVC") {
    linkerFamily = LinkerFamily::MSVC;
  } else if (json == "CLANG") {
    linkerFamily = LinkerFamily::CLANG;
  }
}

void from_json(const Json &json, Compiler &compiler) {
  compiler.path = json.at("PATH").get<std::string>();
  compiler.compilerFamily = json.at("FAMILY").get<CompilerFamily>();
  compiler.compilerVersion = json.at("VERSION").get<Version>();
}

void from_json(const Json &json, Linker &linker) {
  linker.path = json.at("PATH").get<std::string>();
  linker.linkerFamily = json.at("FAMILY").get<LinkerFamily>();
  linker.linkerVersion = json.at("VERSION").get<Version>();
}

void to_json(Json &j, const LibraryType &libraryType) {
  if (libraryType == LibraryType::STATIC) {
    j = "STATIC";
  } else {
    j = "SHARED";
  }
}

void from_json(const Json &json, LibraryType &libraryType) {
  if (json == "STATIC") {
    libraryType = LibraryType::STATIC;
  } else if (json == "SHARED") {
    libraryType = LibraryType::SHARED;
  }
}

void to_json(Json &j, const ConfigType &configType) {
  if (configType == ConfigType::DEBUG) {
    j = "DEBUG";
  } else {
    j = "RELEASE";
  }
}

void from_json(const Json &json, ConfigType &configType) {
  if (json == "DEBUG") {
    configType = ConfigType::DEBUG;
  } else if (json == "RELEASE") {
    configType = ConfigType::RELEASE;
  }
}

Target::Target(std::string targetName_, const Variant &variant)
    : targetName(std::move(targetName_)), outputName(targetName) {
  configurationType = variant.configurationType;
  compiler = variant.compiler;
  linker = variant.linker;
  flags = variant.flags;
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

std::vector<std::string> Target::convertCustomTargetsToJson(const std::vector<CustomTarget> &customTargets, VariantMode mode) {
  std::vector<std::string> commands;
  for (auto &i : customTargets) {
    if (i.mode == mode) {
      commands.push_back(i.command);
    }
  }
  return commands;
}

Json Target::convertToJson(int variantIndex) const {
  Json targetFileJson;

  std::vector<std::string> sourceFilesArray;
  std::vector<Json> sourceDirectoriesArray;
  std::vector<std::string> includeDirectories;
  std::string compilerFlags;
  std::string linkerFlags;
  std::vector<Json> compileDefinitionsArray;
  std::vector<Json> dependenciesArray;

  for (const auto &e : sourceFiles) {
    sourceFilesArray.push_back(e.path.string());
  }
  for (const auto &e : sourceDirectories) {
    Json sourceDirectory;
    sourceDirectory["PATH"] = e.sourceDirectory.path;
    sourceDirectory["REGEX_STRING"] = e.regex;
    sourceDirectoriesArray.push_back(sourceDirectory);
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
          includeDirectories.push_back(e.includeDirectory.path.string());
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
      libDepObject["PATH"] = Cache::configureDirectory.path / std::to_string(variantIndex)
          / library.targetName / fs::path(library.targetName + ".hmake");

      dependenciesArray.push_back(libDepObject);
    } else {
      auto populateDependencies = [&](const PLibrary &pLibrary) {
        for (const auto &e : pLibrary.includeDirectoryDependencies) {
          includeDirectories.push_back(e.path.string());
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

  targetFileJson["TARGET_TYPE"] = targetType;
  targetFileJson["OUTPUT_NAME"] = outputName;
  targetFileJson["OUTPUT_DIRECTORY"] = outputDirectory.path.string();
  targetFileJson["CONFIGURATION"] = configurationType;
  targetFileJson["COMPILER"] = compiler;
  targetFileJson["LINKER"] = linker;
  targetFileJson["COMPILER_FLAGS"] = flags.compilerFlags[compiler.compilerFamily][configurationType];
  targetFileJson["LINKER_FLAGS"] = flags.linkerFlags[linker.linkerFamily][configurationType];
  targetFileJson["SOURCE_FILES"] = sourceFilesArray;
  targetFileJson["SOURCE_DIRECTORIES"] = sourceDirectoriesArray;
  targetFileJson["LIBRARY_DEPENDENCIES"] = dependenciesArray;
  targetFileJson["INCLUDE_DIRECTORIES"] = includeDirectories;
  targetFileJson["COMPILER_TRANSITIVE_FLAGS"] = compilerFlags;
  targetFileJson["LINKER_TRANSITIVE_FLAGS"] = linkerFlags;
  targetFileJson["COMPILE_DEFINITIONS"] = compileDefinitionsArray;
  targetFileJson["PRE_BUILD_CUSTOM_COMMANDS"] = convertCustomTargetsToJson(preBuild, VariantMode::PROJECT);
  targetFileJson["POST_BUILD_CUSTOM_COMMANDS"] = convertCustomTargetsToJson(postBuild, VariantMode::PROJECT);
  targetFileJson["VARIANT"] = "PROJECT";
  return targetFileJson;
}

void to_json(Json &j, const TargetType &p) {
  if (p == TargetType::EXECUTABLE) {
    j = "EXECUTABLE";
  } else if (p == TargetType::STATIC) {
    j = "STATIC";
  } else if (p == TargetType::SHARED) {
    j = "SHARED";
  } else if (p == TargetType::PLIBRARY_SHARED) {
    j = "PLIBRARY_STATIC";
  } else {
    j = "PLIBRARY_SHARED";
  }
}

void Target::configure(int variantIndex) const {
  fs::path targetFileDir = Cache::configureDirectory.path / std::to_string(variantIndex) / targetName;
  fs::create_directory(targetFileDir);
  if (outputDirectory.path.empty()) {
    const_cast<Directory &>(outputDirectory) = Directory(targetFileDir);
  }

  Json targetFileJson;
  targetFileJson = convertToJson(variantIndex);

  fs::path targetFilePath = targetFileDir / (targetName + ".hmake");
  std::ofstream(targetFilePath) << targetFileJson.dump(4);

  for (const auto &libDep : libraryDependencies) {
    if (libDep.ldlt == LDLT::LIBRARY) {
      libDep.library.setTargetType();
      libDep.library.configure(variantIndex);
    }
  }
}

Json Target::convertToJson(const Package &package, const PackageVariant &variant, int count) const {
  Json targetFileJson;

  std::vector<std::string> sourceFilesArray;
  std::vector<Json> sourceDirectoriesArray;
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
  for (const auto &e : sourceDirectories) {
    Json sourceDirectory;
    sourceDirectory["PATH"] = e.sourceDirectory.path;
    sourceDirectory["REGEX_STRING"] = e.regex;
    sourceDirectoriesArray.push_back(sourceDirectory);
  }
  for (const auto &e : includeDirectoryDependencies) {
    Json JIDDObject;
    JIDDObject["PATH"] = e.includeDirectory.path.string();
    if (e.dependencyType == DependencyType::PUBLIC) {
      if (package.cacheCommonIncludeDirs && e.includeDirectory.isCommon) {
        consumerIncludeDirectories.emplace_back(
            "../../Common/"
            + std::to_string(e.includeDirectory.commonDirectoryNumber) + "/include/");
        JIDDObject["COPY"] = false;
      } else {
        consumerIncludeDirectories.emplace_back("include/");
        JIDDObject["COPY"] = true;
      }
    } else {
      JIDDObject["COPY"] = false;
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
      libDepObject["PATH"] = library.getTargetVariantDirectoryPath(count) / (library.targetName + ".hmake");
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
        libDepObject["HMAKE_FILE_PATH"] = pLibrary.getTargetVariantDirectoryPath(count) / (pLibrary.libraryName + ".hmake");
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
          libDepObject["HMAKE_FILE_PATH"] = ppLibrary.getTargetVariantDirectoryPath(count) / (ppLibrary.libraryName + ".hmake");
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

  targetFileJson["TARGET_TYPE"] = targetType;
  targetFileJson["OUTPUT_NAME"] = outputName;
  targetFileJson["OUTPUT_DIRECTORY"] = getTargetVariantDirectoryPath(count);
  targetFileJson["CONFIGURATION"] = configurationType;
  targetFileJson["COMPILER"] = compiler;
  targetFileJson["LINKER"] = linker;
  targetFileJson["COMPILER_FLAGS"] = flags.compilerFlags[compiler.compilerFamily][configurationType];
  targetFileJson["LINKER_FLAGS"] = flags.linkerFlags[linker.linkerFamily][configurationType];
  targetFileJson["SOURCE_FILES"] = sourceFilesArray;
  targetFileJson["SOURCE_DIRECTORIES"] = sourceDirectoriesArray;
  targetFileJson["LIBRARY_DEPENDENCIES"] = dependenciesArray;
  targetFileJson["INCLUDE_DIRECTORIES"] = includeDirectoriesArray;
  targetFileJson["COMPILER_TRANSITIVE_FLAGS"] = compilerFlags;
  targetFileJson["LINKER_TRANSITIVE_FLAGS"] = linkerFlags;
  targetFileJson["COMPILE_DEFINITIONS"] = compileDefinitionsArray;
  targetFileJson["PRE_BUILD_CUSTOM_COMMANDS"] = convertCustomTargetsToJson(preBuild, VariantMode::PACKAGE);
  targetFileJson["POST_BUILD_CUSTOM_COMMANDS"] = convertCustomTargetsToJson(postBuild, VariantMode::PACKAGE);

  Json consumerDependenciesJson;

  consumerDependenciesJson["LIBRARY_TYPE"] = targetType;
  consumerDependenciesJson["LIBRARY_DEPENDENCIES"] = consumerDependenciesArray;
  consumerDependenciesJson["INCLUDE_DIRECTORIES"] = consumerIncludeDirectories;
  consumerDependenciesJson["COMPILER_TRANSITIVE_FLAGS"] = consumerCompilerFlags;
  consumerDependenciesJson["LINKER_TRANSITIVE_FLAGS"] = consumerLinkerFlags;
  consumerDependenciesJson["COMPILE_DEFINITIONS"] = consumerCompileDefinitionsArray;

  targetFileJson["CONSUMER_DEPENDENCIES"] = consumerDependenciesJson;
  targetFileJson["VARIANT"] = "PACKAGE";
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
  fs::path filePath = targetFileBuildDir / (targetName + ".hmake");
  std::ofstream(filePath.string()) << targetFileJson.dump(4);
  for (const auto &libDep : libraryDependencies) {
    if (libDep.ldlt == LDLT::LIBRARY) {
      libDep.library.setTargetType();
      libDep.library.configure(package, variant, count);
    } else if (libDep.ldlt == LDLT::PLIBRARY) {
      libDep.pLibrary.setTargetType();
      libDep.pLibrary.configure(package, variant, count);
    } else if (!libDep.ppLibrary.imported) {
      libDep.ppLibrary.setTargetType();
      libDep.ppLibrary.configure(package, variant, count);
    }
  }
}

Executable::Executable(std::string targetName_, const Variant &variant) : Target(std::move(targetName_), variant) {
  targetType = TargetType::EXECUTABLE;
}

void Executable::assignDifferentVariant(const Variant &variant) {
  Target::assignDifferentVariant(variant);
}

Library::Library(std::string targetName_, const Variant &variant)
    : libraryType(variant.libraryType), Target(std::move(targetName_), variant) {
}

void Library::assignDifferentVariant(const Variant &variant) {
  Target::assignDifferentVariant(variant);
  libraryType = variant.libraryType;
}

void Library::setTargetType() const {
  if (libraryType == LibraryType::STATIC) {
    targetType = TargetType::STATIC;
  } else {
    targetType = TargetType::SHARED;
  }
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

//So that the type becomes compatible for usage  in key of map
bool operator<(const Version &lhs, const Version &rhs) {
  if (lhs.majorVersion < rhs.majorVersion) {
    return true;
  } else if (lhs.majorVersion == rhs.majorVersion) {
    if (lhs.minorVersion < rhs.minorVersion) {
      return true;
    } else if (lhs.minorVersion == rhs.minorVersion) {
      if (lhs.patchVersion < rhs.patchVersion) {
        return true;
      }
    }
  }
  return false;
}

bool operator==(const Version &lhs, const Version &rhs) {
  return lhs.majorVersion == rhs.majorVersion && lhs.minorVersion == rhs.minorVersion && lhs.patchVersion == rhs.patchVersion;
}

CompilerFlags &CompilerFlags::operator[](CompilerFamily compilerFamily) const {
  compilerHelper = true;
  compilerCurrent = compilerFamily;
  return const_cast<CompilerFlags &>(*this);
}

CompilerFlags &CompilerFlags::operator[](CompilerVersion compilerVersion) const {
  if (!compilerHelper) {
    throw std::runtime_error("Wrong Usage Of CompilerFlags class");
  }
  compilerVersionHelper = true;
  compilerVersionCurrent = compilerVersion;
  return const_cast<CompilerFlags &>(*this);
}

CompilerFlags &CompilerFlags::operator[](ConfigType configType) const {
  configHelper = true;
  configCurrent = configType;
  return const_cast<CompilerFlags &>(*this);
}

void CompilerFlags::operator=(const std::string &flags1) {

  std::set<CompilerFamily> compilerFamilies1;
  CompilerVersion compilerVersions1;
  std::set<ConfigType> configurationTypes1;

  if (compilerHelper) {
    compilerFamilies1.emplace(compilerCurrent);
  } else {
    compilerFamilies1 = compilerFamilies;
  }

  if (compilerVersionHelper) {
    compilerVersions1 = compilerVersionCurrent;
  } else {
    CompilerVersion compilerVersion;
    compilerVersion.majorVersion = compilerVersion.minorVersion = compilerVersion.patchVersion = 0;
    compilerVersion.comparison = Comparison::GREATER_THAN_OR_EQUAL_TO;
    compilerVersions1 = compilerVersion;
  }

  if (configHelper) {
    configurationTypes1.emplace(configCurrent);
  } else {
    configurationTypes1 = configurationTypes;
  }

  for (auto &compiler : compilerFamilies1) {
    for (auto &configuration : configurationTypes1) {
      auto t = std::make_tuple(compiler, compilerVersions1, configuration);
      if (auto [pos, ok] = compilerFlags.emplace(t, flags1); !ok) {
        std::cout << "Rewriting the flags in compilerFlags for this configuration";
        compilerFlags[t] = flags1;
      }
    }
  }

  compilerHelper = false;
  compilerVersionHelper = false;
  configHelper = false;
}

CompilerFlags CompilerFlags::defaultFlags() {
  CompilerFlags compilerFlags;
  compilerFlags.compilerFamilies.emplace(CompilerFamily::GCC);
  compilerFlags[CompilerFamily::GCC][CompilerVersion{10, 2, 0}][ConfigType::DEBUG] = "-g";
  compilerFlags[ConfigType::RELEASE] = "-O3 -DNDEBUG";
  return compilerFlags;
}

CompilerFlags::operator std::string() const {
  std::set<std::string> flagsSet;
  for (const auto &[key, value] : compilerFlags) {
    if (compilerHelper) {
      if (std::get<0>(key) != compilerCurrent) {
        continue;
      }
      if (compilerVersionHelper) {
        const auto &compilerVersion = std::get<1>(key);
        if (compilerVersion.comparison == Comparison::EQUAL && compilerVersion == compilerVersionCurrent) {

        } else if (compilerVersion.comparison == Comparison::GREATER_THAN && compilerVersion > compilerVersionCurrent) {

        } else if (compilerVersion.comparison == Comparison::LESSER_THAN && compilerVersion < compilerVersionCurrent) {

        } else if (compilerVersion.comparison == Comparison::GREATER_THAN_OR_EQUAL_TO && compilerVersion >= compilerVersionCurrent) {

        } else if (compilerVersion.comparison == Comparison::LESSER_THAN_OR_EQUAL_TO && compilerVersion <= compilerVersionCurrent) {

        } else {
          continue;
        }
      }
    }
    if (configHelper && std::get<2>(key) != configCurrent) {
      continue;
    }
    flagsSet.emplace(value);
  }

  std::string flagsStr;
  for (const auto &i : flagsSet) {
    flagsStr += i;
    flagsStr += " ";
  }
  return flagsStr;
}

LinkerFlags &LinkerFlags::operator[](LinkerFamily linkerFamily) const {
  linkerHelper = true;
  linkerCurrent = linkerFamily;
  return const_cast<LinkerFlags &>(*this);
}

LinkerFlags &LinkerFlags::operator[](LinkerVersion linkerVersion) const {
  if (!linkerHelper) {
    throw std::runtime_error("Wrong Usage Of LinkerFlags class");
  }
  linkerVersionHelper = true;
  linkerVersionCurrent = linkerVersion;
  return const_cast<LinkerFlags &>(*this);
}

LinkerFlags &LinkerFlags::operator[](ConfigType configType) const {
  configHelper = true;
  configCurrent = configType;
  return const_cast<LinkerFlags &>(*this);
}

void LinkerFlags::operator=(const std::string &flags1) {

  std::set<LinkerFamily> linkerFamilies1;
  LinkerVersion linkerVersions1;
  std::set<ConfigType> configurationTypes1;

  if (linkerHelper) {
    linkerFamilies1.emplace(linkerCurrent);
  } else {
    linkerFamilies1 = linkerFamilies;
  }

  if (linkerVersionHelper) {
    linkerVersions1 = linkerVersionCurrent;
  } else {
    LinkerVersion linkerVersion;
    linkerVersion.minorVersion = linkerVersion.minorVersion = linkerVersion.patchVersion = 0;
    linkerVersion.comparison = Comparison::GREATER_THAN_OR_EQUAL_TO;
    linkerVersions1 = linkerVersion;
  }

  if (configHelper) {
    configurationTypes1.emplace(configCurrent);
  } else {
    configurationTypes1 = configurationTypes;
  }

  for (auto &linker : linkerFamilies1) {
    for (auto &configuration : configurationTypes1) {
      auto t = std::make_tuple(linker, linkerVersions1, configuration);
      if (auto [pos, ok] = linkerFlags.emplace(t, flags1); !ok) {
        std::cout << "Rewriting the flags in compilerFlags for this configuration";
        linkerFlags[t] = flags1;
      }
    }
  }

  linkerHelper = false;
  linkerVersionHelper = false;
  configHelper = false;
}

LinkerFlags LinkerFlags::defaultFlags() {
  return LinkerFlags{};
}

LinkerFlags::operator std::string() const {
  std::set<std::string> flagsSet;
  for (const auto &[key, value] : linkerFlags) {
    if (linkerHelper) {
      if (std::get<0>(key) != linkerCurrent) {
        continue;
      }
      if (linkerVersionHelper) {
        const auto &linkerVersion = std::get<1>(key);
        if (linkerVersion.comparison == Comparison::EQUAL && linkerVersion == linkerVersionCurrent) {

        } else if (linkerVersion.comparison == Comparison::GREATER_THAN && linkerVersion > linkerVersionCurrent) {

        } else if (linkerVersion.comparison == Comparison::LESSER_THAN && linkerVersion < linkerVersionCurrent) {

        } else if (linkerVersion.comparison == Comparison::GREATER_THAN_OR_EQUAL_TO && linkerVersion >= linkerVersionCurrent) {

        } else if (linkerVersion.comparison == Comparison::LESSER_THAN_OR_EQUAL_TO && linkerVersion <= linkerVersionCurrent) {

        } else {
          continue;
        }
      }
    }
    if (configHelper && std::get<2>(key) != configCurrent) {
      continue;
    }
    flagsSet.emplace(value);
  }

  std::string flagsStr;
  for (const auto &i : flagsStr) {
    flagsStr += i;
    flagsStr += " ";
  }
  return flagsStr;
}

void Cache::initializeCache() {
  fs::path filePath = fs::current_path() / "cache.hmake";
  Json cacheFileJson;
  std::ifstream(filePath) >> cacheFileJson;

  fs::path srcDirPath = cacheFileJson.at("SOURCE_DIRECTORY").get<std::string>();
  if (srcDirPath.is_relative()) {
    srcDirPath = (fs::current_path() / srcDirPath).lexically_normal();
  }
  Cache::sourceDirectory = Directory(srcDirPath);
  Cache::configureDirectory = Directory(fs::current_path());

  Cache::copyPackage = cacheFileJson.at("PACKAGE_COPY").get<bool>();
  if (copyPackage) {
    packageCopyPath = cacheFileJson.at("PACKAGE_COPY_PATH").get<std::string>();
  }

  Cache::projectConfigurationType = cacheFileJson.at("CONFIGURATION").get<ConfigType>();
  Cache::compilerArray = cacheFileJson.at("COMPILER_ARRAY").get<std::vector<Compiler>>();
  Cache::selectedCompilerArrayIndex = cacheFileJson.at("COMPILER_SELECTED_ARRAY_INDEX").get<int>();
  Cache::linkerArray = cacheFileJson.at("LINKER_ARRAY").get<std::vector<Linker>>();
  Cache::selectedLinkerArrayIndex = cacheFileJson.at("COMPILER_SELECTED_ARRAY_INDEX").get<int>();
  Cache::libraryType = cacheFileJson.at("LIBRARY_TYPE").get<LibraryType>();
  Cache::cacheVariables = cacheFileJson.at("CACHE_VARIABLES").get<Json>();
}

void Cache::registerCacheVariables() {
  fs::path filePath = fs::current_path() / "cache.hmake";
  Json cacheFileJson;
  std::ifstream(filePath) >> cacheFileJson;
  cacheFileJson["CACHE_VARIABLES"] = Cache::cacheVariables;
  std::ofstream(filePath) << cacheFileJson.dump(2);
}

Variant::Variant() {
  configurationType = Cache::projectConfigurationType;
  compiler = Cache::compilerArray[Cache::selectedCompilerArrayIndex];
  linker = Cache::linkerArray[Cache::selectedLinkerArrayIndex];
  libraryType = Cache::libraryType;
  flags = ::flags;
}

Json Variant::convertToJson(VariantMode mode, int variantCount) {
  Json projectJson;
  projectJson["CONFIGURATION"] = configurationType;
  projectJson["COMPILER"] = compiler;
  projectJson["LINKER"] = linker;
  std::string compilerFlags = flags.compilerFlags[compiler.compilerFamily][configurationType];
  projectJson["COMPILER_FLAGS"] = compilerFlags;
  std::string linkerFlags = flags.linkerFlags[linker.linkerFamily][configurationType];
  projectJson["LINKER_FLAGS"] = linkerFlags;
  projectJson["LIBRARY_TYPE"] = libraryType;

  std::vector<Json> targetArray;
  for (const auto &exe : executables) {
    std::string path;
    if (mode == VariantMode::PROJECT) {
      path = (Cache::configureDirectory.path / std::to_string(variantCount)
          / fs::path(exe.targetName) / (exe.targetName + ".hmake")).string();
    } else {
      path = (exe.getTargetVariantDirectoryPath(variantCount) / (exe.targetName + ".hmake")).string();
    }
    targetArray.emplace_back(path);
  }
  for (const auto &lib : libraries) {
    std::string path;
    if (mode == VariantMode::PROJECT) {
      path = (Cache::configureDirectory.path / std::to_string(variantCount)
          / fs::path(lib.targetName) / (lib.targetName + ".hmake")).string();
    } else {
      path = (lib.getTargetVariantDirectoryPath(variantCount) / (lib.targetName + ".hmake")).string();
    }
    targetArray.emplace_back(path);
  }
  projectJson["TARGETS"] = targetArray;
  return projectJson;
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

void Project::configure() {
  for (int i = 0; i < projectVariants.size(); ++i) {
    Json json = projectVariants[i].convertToJson(VariantMode::PROJECT, i);
    fs::path variantFileDir = Cache::configureDirectory.path / std::to_string(i);
    fs::create_directory(variantFileDir);
    fs::path projectFilePath = variantFileDir / "projectVariant.hmake";
    std::ofstream(projectFilePath) << json.dump(4);
    for (auto &exe : projectVariants[i].executables) {
      exe.configure(i);
    }
    for (auto &lib : projectVariants[i].libraries) {
      lib.setTargetType();
      lib.configure(i);
    }
  }
  Json projectFileJson;
  std::vector<std::string> projectVariantsInt;
  projectVariantsInt.reserve(projectVariants.size());
  for (int i = 0; i < projectVariants.size(); ++i) {
    projectVariantsInt.push_back(std::to_string(i));
  }
  projectFileJson["VARIANTS"] = projectVariantsInt;
  std::ofstream(Cache::configureDirectory.path / "project.hmake") << projectFileJson.dump(4);
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

  for (int i = 0; i < packageVariants.size(); ++i) {
    for (const auto &exe : packageVariants[i].executables) {
      assignPackageCommonAndNonCommonIncludeDirs(&exe, i);
    }
    for (const auto &lib : packageVariants[i].libraries) {
      assignPackageCommonAndNonCommonIncludeDirs(&lib, i);
    }
  }

  Json commonDirsJson;
  for (int i = 0; i < packageCommonIncludeDirs.size(); ++i) {
    for (auto j : packageCommonIncludeDirs[i].directories) {
      j->isCommon = true;
      j->commonDirectoryNumber = i;
    }

    Json commonDirJsonObject;
    commonDirJsonObject["INDEX"] = i;
    commonDirJsonObject["PATH"] = packageCommonIncludeDirs[i].directories[0]->path;
    commonDirJsonObject["VARIANTS_INDICES"] = packageCommonIncludeDirs[i].variantsIndices;
    commonDirsJson.emplace_back(commonDirJsonObject);
  }
  fs::create_directory(Cache::configureDirectory.path / "Package");
  std::ofstream(Cache::configureDirectory.path / "Package" / fs::path("Common.hmake")) << commonDirsJson.dump(4);
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
  packageFileJson["CACHE_INCLUDES"] = cacheCommonIncludeDirs;
  packageFileJson["PACKAGE_COPY"] = Cache::copyPackage;
  packageFileJson["PACKAGE_COPY_PATH"] = Cache::packageCopyPath;
  packageFileJson["VARIANTS"] = packageVariantJson;
  fs::path packageDirectorypath = Cache::configureDirectory.path / "Package";
  fs::create_directory(packageDirectorypath);
  Directory packageDirectory(packageDirectorypath);
  fs::path file = packageDirectory.path / "package.hmake";
  std::ofstream(file) << packageFileJson.dump(4);

  if (Cache::copyPackage && cacheCommonIncludeDirs) {
    configureCommonAmongVariants();
  }
  count = 0;
  for (auto &variant : packageVariants) {
    Json variantJsonFile = variant.convertToJson(VariantMode::PACKAGE, count);
    fs::path variantFileDir = packageDirectory.path / std::to_string(count);
    fs::create_directory(variantFileDir);
    fs::path variantFilePath = variantFileDir / "packageVariant.hmake";
    std::ofstream(variantFilePath) << variantJsonFile.dump(4);
    for (const auto &exe : variant.executables) {
      exe.configure(*this, variant, count);
    }
    for (const auto &lib : variant.libraries) {
      lib.setTargetType();
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

PLibrary::PLibrary(const fs::path &libraryPath_, LibraryType libraryType_)
    : libraryType(libraryType_) {
  if (libraryPath_.is_relative()) {
    libraryPath = Cache::sourceDirectory.path / libraryPath_;
    libraryPath = libraryPath.lexically_normal();
  }
  if (libraryType == LibraryType::STATIC) {
    libraryName = libraryPath.filename().string();
    libraryName.erase(0, 3);
    libraryName.erase(libraryName.find('.'), 2);
  }
}

fs::path PLibrary::getTargetVariantDirectoryPath(int variantCount) const {
  return Cache::configureDirectory.path / "Package" / std::to_string(variantCount) / libraryName;
}

void PLibrary::setTargetType() const {
  if (libraryType == LibraryType::STATIC) {
    targetType = TargetType::PLIBRARY_STATIC;
  } else {
    targetType = TargetType::PLIBRARY_SHARED;
  }
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
    if (package.cacheCommonIncludeDirs && e.isCommon) {
      consumerIncludeDirectories.emplace_back(
          "../../Common/"
          + std::to_string(e.commonDirectoryNumber) + "/include/");
      JIDDObject["COPY"] = false;
    } else {
      consumerIncludeDirectories.emplace_back("include/");
      JIDDObject["COPY"] = true;
    }
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
      libDepObject["PATH"] = library.getTargetVariantDirectoryPath(count) / (library.targetName + ".hmake");
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

        libDepObject["PATH"] = pLibrary.libraryPath;
        libDepObject["IMPORTED"] = false;
        libDepObject["HMAKE_FILE_PATH"] = pLibrary.getTargetVariantDirectoryPath(count) / (pLibrary.libraryName + ".hmake");
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
          libDepObject["HMAKE_FILE_PATH"] = ppLibrary.getTargetVariantDirectoryPath(count) / (ppLibrary.libraryName + ".hmake");
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

  targetFileJson["TARGET_TYPE"] = targetType;
  targetFileJson["LIBRARY_DEPENDENCIES"] = dependenciesArray;
  targetFileJson["INCLUDE_DIRECTORIES"] = includeDirectoriesArray;

  Json consumerDependenciesJson;
  consumerDependenciesJson["LIBRARY_TYPE"] = libraryType;
  consumerDependenciesJson["LIBRARY_DEPENDENCIES"] = consumerDependenciesArray;
  consumerDependenciesJson["INCLUDE_DIRECTORIES"] = consumerIncludeDirectories;
  consumerDependenciesJson["COMPILER_TRANSITIVE_FLAGS"] = consumerCompilerFlags;
  consumerDependenciesJson["LINKER_TRANSITIVE_FLAGS"] = consumerLinkerFlags;
  consumerDependenciesJson["COMPILE_DEFINITIONS"] = consumerCompileDefinitionsArray;

  targetFileJson["CONSUMER_DEPENDENCIES"] = consumerDependenciesJson;
  targetFileJson["VARIANT"] = "PACKAGE";
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
  fs::path filePath = targetFileBuildDir / (libraryName + ".hmake");
  std::ofstream(filePath.string()) << targetFileJson.dump(4);
  for (const auto &libDep : libraryDependencies) {
    if (libDep.ldlt == LDLT::LIBRARY) {
      libDep.library.setTargetType();
      libDep.library.configure(package, variant, count);
    } else if (libDep.ldlt == LDLT::PLIBRARY) {
      libDep.pLibrary.setTargetType();
      libDep.pLibrary.configure(package, variant, count);
    } else if (!libDep.ppLibrary.imported) {
      libDep.ppLibrary.setTargetType();
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

  fs::path libraryFilePath = libraryDirectoryPath / (libraryName + ".hmake");
  if (!fs::exists(libraryFilePath)) {
    throw std::runtime_error("Library " + libraryName
                             + " Does Not Exists. Searched For File " + libraryFilePath.string());
  }

  Json libraryFileJson;
  std::ifstream(libraryFilePath) >> libraryFileJson;

  if (libraryFileJson.at("LIBRARY_TYPE").get<std::string>() == "STATIC") {
    libraryType = LibraryType::STATIC;
  } else {
    libraryType = LibraryType::SHARED;
  }
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