
#include "BBuild.hpp"
#include "fstream"
#include "iostream"

BTarget::BTarget(const std::string &targetFilePath) {

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

  outputName = targetFileJson.at("OUTPUT_NAME").get<std::string>();
  outputDirectory = targetFileJson.at("OUTPUT_DIRECTORY").get<fs::path>();
  compilerPath = targetFileJson.at("COMPILER").get<JObject>().at("PATH").get<std::string>();
  linkerPath = targetFileJson.at("LINKER").get<JObject>().at("PATH").get<std::string>();
  compilerFlags = targetFileJson.at("COMPILER_FLAGS").get<std::string>();
  linkerFlags = targetFileJson.at("LINKER_FLAGS").get<std::string>();
  sourceFiles = targetFileJson.at("SOURCE_FILES").get<std::vector<std::string>>();
  assignSpecialBCopyableDependencyVector("LIBRARY_DEPENDENCIES", targetFileJson, libraryDependencies);
  assignSpecialBCopyableDependencyVector("INCLUDE_DIRECTORIES", targetFileJson, includeDirectories);
  compilerTransitiveFlags = targetFileJson.at("COMPILER_TRANSITIVE_FLAGS").get<std::string>();
  linkerTransitiveFlags = targetFileJson.at("LINKER_TRANSITIVE_FLAGS").get<std::string>();
  buildCacheFilesDirPath = fs::path(targetFilePath).parent_path() / ("Cache_Build_Files");
}

void BTarget::build(const fs::path &copyPath, bool copyTarget) {

  std::string includeDirectoriesFlags;
  std::string libraryDependenciesFlags;

  for (const auto &i : includeDirectories) {
    includeDirectoriesFlags.append("-I " + i.path + " ");
  }

  for (const auto &i : libraryDependencies) {

    BTarget buildTarget(i.path);
    buildTarget.build(copyPath, copyTarget);
    libraryDependenciesFlags.append("-L" + buildTarget.outputDirectory + " -l" + buildTarget.outputName + " ");
  }

  //Build process starts
  std::cout << "Starting Building" << std::endl;
  std::cout << "Building From Start. Does Not Cache Builds Yet." << std::endl;
  fs::create_directory(outputDirectory);
  fs::create_directory(buildCacheFilesDirPath);
  for (const auto &i : sourceFiles) {
    std::string compileCommand = compilerPath + " ";
    compileCommand += compilerFlags + " ";
    compileCommand += compilerTransitiveFlags + " " + includeDirectoriesFlags;
    compileCommand += " -c " + i
        + " -o " + (fs::path(buildCacheFilesDirPath) / fs::path(i).filename()).string() + ".o";
    std::cout << compileCommand << std::endl;
    system(compileCommand.c_str());
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
  system(linkerCommand.c_str());
  std::cout << "Built Complete" << std::endl;

  if (copyTarget) {
    std::cout << "Copying Started" << std::endl;
    copy(copyPath);
    std::cout << "Copying Completed" << std::endl;
  }
}

void BTarget::copy(const fs::path &copyPath) {
  fs::path copyFrom;
  fs::path copyTo = copyPath / targetName / "";
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
      fs::path includeDirectoryCopyTo = copyPath / targetName / "include";
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

void BTarget::assignSpecialBCopyableDependencyVector(const std::string &jString, const Json &json,
                                                     std::vector<BCopyableDependency> &container) {
  JArray tmpVector = json.at(jString).get<JArray>();
  for (auto &i : tmpVector) {
    BCopyableDependency dependency;
    dependency.path = i.at("PATH").get<std::string>();
    if (i.contains("COPY")) {
      dependency.copy = i.at("COPY").get<bool>();
    } else {
      dependency.copy = false;
    }
    container.push_back(dependency);
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

void BPackageVariant::buildAndCopy(const fs::path &copyPath) {
  for (const auto &t : targetFilePaths) {
    BTarget builder(t);
    builder.build(copyPath, true);
  }
}

bool BPackageVariant::shouldVariantBeCopied() {
  fs::path packagePath = fs::current_path().parent_path() / "package.hmake";
  Json packageJson;
  std::ifstream(packagePath) >> packageJson;
  return packageJson.at("PACKAGE_COPY").get<bool>();
}

BPackage::BPackage(const fs::path &packageFilePath) {
  Json packageFileJson;
  std::ifstream(packageFilePath) >> packageFileJson;
  JArray variants = packageFileJson.at("VARIANTS").get<JArray>();
  fs::path packageCopyPath;
  bool packageCopy = packageFileJson.at("PACKAGE_COPY").get<bool>();
  if (packageCopy) {
    std::string packageName = packageFileJson.at("NAME").get<std::string>();
    packageCopyPath = packageFileJson.at("PACKAGE_COPY_PATH").get<std::string>();
    packageCopyPath /= packageName;
  }
  for (auto &variant : variants) {
    std::string integerIndex = variant.at("INDEX").get<std::string>();
    fs::path packageVariantFilePath = fs::current_path() / integerIndex / "packageVariant.hmake";
    if (!is_regular_file(packageVariantFilePath)) {
      throw std::runtime_error(packageVariantFilePath.string() + " is not a regular file");
    }
    BPackageVariant packageVariant(packageVariantFilePath);
    if (packageCopy) {
      packageVariant.buildAndCopy(packageCopyPath / integerIndex);
    } else {
      packageVariant.build();
    }
  }
}
