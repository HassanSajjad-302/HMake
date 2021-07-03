
#include "BBuild.hpp"
#include "fstream"
#include "iostream"

void from_json(const Json &j, BIDD &p) {
  p.copy = j.at("COPY").get<bool>();
  p.path = j.at("PATH").get<std::string>();
}

void from_json(const Json &j, BLibraryDependency &p) {
  p.preBuilt = j.at("PREBUILT").get<bool>();
  p.path = j.at("PATH").get<std::string>();
  if (p.preBuilt) {
    if (j.contains("IMPORTED")) {
      p.imported = j.at("IMPORTED").get<bool>();
      if (!p.imported) {
        p.hmakeFilePath = j.at("HMAKE_FILE_PATH").get<std::string>();
      }
    } else {
      //hbuild was ran in project variant mode.
      p.imported = true;
    }
  }
  p.copy = true;
}

void from_json(const Json &j, BCompileDefinition &p) {
  p.name = j.at("NAME").get<std::string>();
  p.value = j.at("VALUE").get<std::string>();
}

BTarget::BTarget(const std::string &targetFilePath) {
  targetFileName = fs::path(targetFilePath).filename();
  if (targetFilePath.ends_with(".executable.hmake")) {
    targetType = BTargetType::EXECUTABLE;
  } else if (targetFilePath.ends_with(".static.hmake")) {
    targetType = BTargetType::STATIC;
  } else {
    targetType = BTargetType::SHARED;
  }

  if (std::string fileName = fs::path(targetFilePath).filename();
      targetType == BTargetType::EXECUTABLE) {
    targetName = fileName.substr(0, fileName.size() - std::string(".executable.hmake").size());
  } else if (targetType == BTargetType::SHARED) {
    targetName = fileName.substr(0, fileName.size() - std::string(".shared.hmake").size());
  } else {
    targetName = fileName.substr(0, fileName.size() - std::string(".static.hmake").size());
  }
  Json targetFileJson;
  std::ifstream(targetFilePath) >> targetFileJson;

  isInPackage = targetFileJson.at("IS_IN_PACKAGE").get<bool>();
  if (isInPackage) {
    copyPackage = targetFileJson.at("PACKAGE_COPY").get<bool>();
    if (copyPackage) {
      packageName = targetFileJson.at("PACKAGE_NAME").get<std::string>();
      packageCopyPath = targetFileJson.at("PACKAGE_COPY_PATH").get<std::string>();
      packageVariantIndex = targetFileJson.at("PACKAGE_VARIANT_INDEX").get<int>();
      packageTargetPath = packageCopyPath / fs::path(packageName)
          / fs::path(std::to_string(packageVariantIndex)) / targetName;
    }
  } else {
    copyPackage = false;
  }
  outputName = targetFileJson.at("OUTPUT_NAME").get<std::string>();
  outputDirectory = targetFileJson.at("OUTPUT_DIRECTORY").get<fs::path>();
  compilerPath = targetFileJson.at("COMPILER").get<Json>().at("PATH").get<std::string>();
  linkerPath = targetFileJson.at("LINKER").get<Json>().at("PATH").get<std::string>();
  compilerFlags = targetFileJson.at("COMPILER_FLAGS").get<std::string>();
  linkerFlags = targetFileJson.at("LINKER_FLAGS").get<std::string>();
  sourceFiles = targetFileJson.at("SOURCE_FILES").get<std::vector<std::string>>();

  libraryDependencies = targetFileJson.at("LIBRARY_DEPENDENCIES").get<std::vector<BLibraryDependency>>();
  if (isInPackage) {
    includeDirectories = targetFileJson.at("INCLUDE_DIRECTORIES").get<std::vector<BIDD>>();
  } else {
    std::vector<std::string> includeDirs = targetFileJson.at("INCLUDE_DIRECTORIES").get<std::vector<std::string>>();
    for (auto &i : includeDirs) {
      includeDirectories.push_back(BIDD{i, true});
    }
  }
  compilerTransitiveFlags = targetFileJson.at("COMPILER_TRANSITIVE_FLAGS").get<std::string>();
  linkerTransitiveFlags = targetFileJson.at("LINKER_TRANSITIVE_FLAGS").get<std::string>();
  if (!targetFileJson.at("COMPILE_DEFINITIONS").empty()) {
    compileDefinitions = targetFileJson.at("COMPILE_DEFINITIONS").get<std::vector<BCompileDefinition>>();
  }
  buildCacheFilesDirPath = fs::path(targetFilePath).parent_path() / ("Cache_Build_Files");
  if (copyPackage) {
    consumerDependenciesJson = targetFileJson.at("CONSUMER_DEPENDENCIES").get<Json>();
  }
}

void BTarget::build() {

  std::string includeDirectoriesFlags;
  std::string libraryDependenciesFlags;
  std::string compileDefinitionsString;

  for (const auto &i : includeDirectories) {
    includeDirectoriesFlags.append("-I " + i.path + " ");
  }

  for (const auto &i : libraryDependencies) {
    if (!i.preBuilt) {
      BTarget buildTarget(i.path);
      buildTarget.build();
      libraryDependenciesFlags.append("-L" + buildTarget.outputDirectory + " -l" + buildTarget.outputName + " ");
    } else {
      std::string dir = fs::path(i.path).parent_path();
      std::string libName = fs::path(i.path).filename();
      libName.erase(0, 3);
      libName.erase(libName.find('.'), 2);
      std::string str = "-L " + dir + " -l";
      libraryDependenciesFlags.append(str + libName + " ");

      if (!i.imported) {
        BPTarget target(i.hmakeFilePath);
        target.build(i.path);
      }
    }
  }

  for (const auto &i : compileDefinitions) {
    compileDefinitionsString += "-D" + i.name + "=" + i.value + " ";
  }
  //Build process starts
  std::cout << "Starting Building" << std::endl;
  std::cout << "Building From Start. Does Not Cache Builds Yet." << std::endl;
  fs::create_directory(outputDirectory);
  fs::create_directory(buildCacheFilesDirPath);
  for (const auto &i : sourceFiles) {
    std::string compileCommand = compilerPath + " ";
    compileCommand += compilerFlags + " ";
    compileCommand += compilerTransitiveFlags + " ";
    compileCommand += compileDefinitionsString + " ";
    compileCommand += includeDirectoriesFlags;
    compileCommand += " -c " + i
        + " -o " + (fs::path(buildCacheFilesDirPath) / fs::path(i).filename()).string() + ".o";
    std::cout << compileCommand << std::endl;
    int code = std::system(compileCommand.c_str());
    if (code != EXIT_SUCCESS) {
      exit(code);
    }
  }

  std::string linkerCommand;
  if (targetType == BTargetType::EXECUTABLE) {
    std::cout << "Linking" << std::endl;
    linkerCommand = linkerPath
        + " " + linkerFlags + " " + linkerTransitiveFlags
        + " " + fs::path(buildCacheFilesDirPath / fs::path("")).string() + "*.o "
        + " " + libraryDependenciesFlags
        + " -o " + (fs::path(outputDirectory) / outputName).string();
  } else if (targetType == BTargetType::STATIC) {
    linkerCommand = "/usr/bin/ar rcs " + (fs::path(outputDirectory) / ("lib" + outputName + ".a")).string()
        + " " + fs::path(buildCacheFilesDirPath / fs::path("")).string() + "*.o ";
  } else {
  }

  std::cout << linkerCommand << std::endl;
  int code = std::system(linkerCommand.c_str());
  if (code != EXIT_SUCCESS) {
    exit(code);
  }
  std::cout << "Built Complete" << std::endl;

  if (copyPackage) {
    std::cout << "Copying Started" << std::endl;
    copy();
    std::string consumerTargetFileName;
    fs::path consumerTargetFile = packageTargetPath / targetFileName;
    std::ofstream(consumerTargetFile) << consumerDependenciesJson.dump(4);
    std::cout << "Copying Completed" << std::endl;
  }
}

void BTarget::copy() {
  fs::path copyFrom;
  fs::path copyTo = packageTargetPath / "";
  if (targetType == BTargetType::EXECUTABLE) {
    copyFrom = fs::path(outputDirectory) / outputName;
  } else if (targetType == BTargetType::STATIC) {
    copyFrom = fs::path(outputDirectory) / ("lib" + outputName + ".a");
  } else {
    //Shared Libraries Not Supported Yet.
  }
  copyFrom = copyFrom.lexically_normal();
  copyTo = copyTo.lexically_normal();
  std::cout << "Copying Target" << std::endl;
  std::cout << "Copying Target From " << copyFrom.string() << std::endl;
  std::cout << "Copying Target To " << copyTo.string() << std::endl;
  fs::create_directories(copyTo);
  fs::copy(copyFrom, copyTo, fs::copy_options::update_existing);
  std::cout << "Target Copying Done" << std::endl;
  std::cout << "Copying Include Directories" << std::endl;
  for (auto &i : includeDirectories) {
    if (i.copy) {
      fs::path includeDirectoryCopyFrom = i.path;
      fs::path includeDirectoryCopyTo = packageTargetPath / "include";
      includeDirectoryCopyFrom = includeDirectoryCopyFrom.lexically_normal();
      includeDirectoryCopyTo = includeDirectoryCopyTo.lexically_normal();
      std::cout << "Copying IncludeDirectory From " << includeDirectoryCopyFrom << std::endl;
      std::cout << "Copying IncludeDirectory To " << includeDirectoryCopyTo << std::endl;
      fs::create_directories(includeDirectoryCopyTo);
      fs::copy(includeDirectoryCopyFrom, includeDirectoryCopyTo,
               fs::copy_options::update_existing | fs::copy_options::recursive);
      std::cout << "IncludeDirectory Copying Done" << std::endl;
    }
  }
}

BPTarget::BPTarget(const std::string &targetFilePath) {

  targetFileName = fs::path(targetFilePath).filename();
  if (targetFilePath.ends_with(".executable.hmake")) {
    targetType = BTargetType::EXECUTABLE;
  } else if (targetFilePath.ends_with(".static.hmake")) {
    targetType = BTargetType::STATIC;
  } else {
    targetType = BTargetType::SHARED;
  }

  if (std::string fileName = fs::path(targetFilePath).filename();
      targetType == BTargetType::EXECUTABLE) {
    targetName = fileName.substr(0, fileName.size() - std::string(".executable.hmake").size());
  } else if (targetType == BTargetType::SHARED) {
    targetName = fileName.substr(0, fileName.size() - std::string(".shared.hmake").size());
  } else {
    targetName = fileName.substr(0, fileName.size() - std::string(".static.hmake").size());
  }
  Json targetFileJson;
  std::ifstream(targetFilePath) >> targetFileJson;

  isInPackage = targetFileJson.at("IS_IN_PACKAGE").get<bool>();
  if (isInPackage) {
    copyPackage = targetFileJson.at("PACKAGE_COPY").get<bool>();
    if (copyPackage) {
      packageName = targetFileJson.at("PACKAGE_NAME").get<std::string>();
      packageCopyPath = targetFileJson.at("PACKAGE_COPY_PATH").get<std::string>();
      packageVariantIndex = targetFileJson.at("PACKAGE_VARIANT_INDEX").get<int>();
      packageTargetPath = packageCopyPath / fs::path(packageName)
          / fs::path(std::to_string(packageVariantIndex)) / targetName;
    }
  } else {
    copyPackage = false;
  }

  libraryDependencies = targetFileJson.at("LIBRARY_DEPENDENCIES").get<std::vector<BLibraryDependency>>();
  if (isInPackage) {
    includeDirectories = targetFileJson.at("INCLUDE_DIRECTORIES").get<std::vector<BIDD>>();
  }

  if (copyPackage) {
    consumerDependenciesJson = targetFileJson.at("CONSUMER_DEPENDENCIES").get<Json>();
  }
}

void BPTarget::build(const fs::path &copyFrom) {
  for (const auto &i : libraryDependencies) {
    if (!i.preBuilt) {
      BTarget buildTarget(i.path);
      buildTarget.build();
    } else {
      if (!i.imported) {
        BPTarget target(i.hmakeFilePath);
        target.build(copyFrom);
      }
    }
  }
  if (copyPackage) {
    std::cout << "Copying Started" << std::endl;
    copy(copyFrom);
    std::string consumerTargetFileName;
    fs::path consumerTargetFile = packageTargetPath / targetFileName;
    std::ofstream(consumerTargetFile) << consumerDependenciesJson.dump(4);
    std::cout << "Copying Completed" << std::endl;
  }
}

void BPTarget::copy(fs::path copyFrom) {
  fs::path copyTo = packageTargetPath / "";
  copyFrom = copyFrom.lexically_normal();
  copyTo = copyTo.lexically_normal();
  std::cout << "Copying Target" << std::endl;
  std::cout << "Copying Target From " << copyFrom.string() << std::endl;
  std::cout << "Copying Target To " << copyTo.string() << std::endl;
  fs::create_directories(copyTo);
  fs::copy(copyFrom, copyTo, fs::copy_options::update_existing);
  std::cout << "Target Copying Done" << std::endl;
  std::cout << "Copying Include Directories" << std::endl;
  for (auto &i : includeDirectories) {
    if (i.copy) {
      fs::path includeDirectoryCopyFrom = i.path;
      fs::path includeDirectoryCopyTo = packageTargetPath / "include";
      includeDirectoryCopyFrom = includeDirectoryCopyFrom.lexically_normal();
      includeDirectoryCopyTo = includeDirectoryCopyTo.lexically_normal();
      std::cout << "Copying IncludeDirectory From " << includeDirectoryCopyFrom << std::endl;
      std::cout << "Copying IncludeDirectory To " << includeDirectoryCopyTo << std::endl;
      fs::create_directories(includeDirectoryCopyTo);
      fs::copy(includeDirectoryCopyFrom, includeDirectoryCopyTo,
               fs::copy_options::update_existing | fs::copy_options::recursive);
      std::cout << "IncludeDirectory Copying Done" << std::endl;
    }
  }
}

BVariant::BVariant(const fs::path &variantFilePath) {
  Json variantFileJson;
  std::ifstream(variantFilePath) >> variantFileJson;
  targetFilePaths = variantFileJson.at("TARGETS").get<std::vector<std::string>>();
}

void BVariant::build() {
  for (const auto &t : targetFilePaths) {
    BTarget builder(t);
    builder.build();
  }
}

BProjectVariant::BProjectVariant(const fs::path &projectFilePath) : BVariant(projectFilePath) {
}

BPackageVariant::BPackageVariant(const fs::path &packageVariantFilePath) : BVariant(packageVariantFilePath) {
}

BPackage::BPackage(const fs::path &packageFilePath) {
  Json packageFileJson;
  std::ifstream(packageFilePath) >> packageFileJson;
  Json variants = packageFileJson.at("VARIANTS").get<Json>();
  fs::path packageCopyPath;
  bool packageCopy = packageFileJson.at("PACKAGE_COPY").get<bool>();
  if (packageCopy) {
    std::string packageName = packageFileJson.at("NAME").get<std::string>();
    std::string version = packageFileJson.at("VERSION").get<std::string>();
    packageCopyPath = packageFileJson.at("PACKAGE_COPY_PATH").get<std::string>();
    packageCopyPath /= packageName;
    fs::path consumePackageFilePath = packageCopyPath / "cpackage.hmake";
    Json consumePackageFilePathJson;
    consumePackageFilePathJson["NAME"] = packageName;
    consumePackageFilePathJson["VERSION"] = version;
    consumePackageFilePathJson["VARIANTS"] = variants;
    fs::create_directories(packageCopyPath);
    std::ofstream(consumePackageFilePath) << consumePackageFilePathJson.dump(4);
  }
  for (auto &variant : variants) {
    std::string integerIndex = variant.at("INDEX").get<std::string>();
    fs::path packageVariantFilePath = fs::current_path() / integerIndex / "packageVariant.hmake";
    if (!is_regular_file(packageVariantFilePath)) {
      throw std::runtime_error(packageVariantFilePath.string() + " is not a regular file");
    }
    BPackageVariant packageVariant(packageVariantFilePath);
    packageVariant.build();
  }
}
