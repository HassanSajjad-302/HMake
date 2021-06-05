#ifndef EXECUTABLE_HPP
#define EXECUTABLE_HPP

#include "CxxStandard.hpp"
#include "Directory.hpp"
#include "File.hpp"
#include "IDD.hpp"
#include "LibraryDependency.hpp"
#include "LinkerFlagsDependency.hpp"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <string>
#include <vector>

struct Executable {
  std::vector<IDD> includeDirectoryDependencies;
  std::vector<LibraryDependency> libraryDependencies;
  std::vector<CompilerFlagsDependency> compilerFlagsDependencies;
  std::vector<LinkerFlagsDependency> linkerFlagsDependencies;
  std::vector<CompileDefinitionDependency> compileDefinitionDependencies;
  std::vector<File> sourceFiles;
  std::string targetName;
  Directory configureDirectory;
  //todo: Why is it not Directory. Rename it to outputDirectoryPath
  fs::path buildDirectoryPath;
  //configureDirectory will be same as project::SOURCE_DIRECTORY. And the executable's build directory will be
  //same as configureDirectory. To specify a different build directory, set the buildDirectoryPath variable.
  Executable(std::string targetName_, File file);

  //This will create a configure directory under the project::BUILD_DIRECTORY.
  Executable(std::string targetName_, File file, fs::path configureDirectoryPathRelativeToProjectBuildPath);

  //TODO: This constructor should not exist. A Project's configure directory
  //should also be the configure directory for all of the targets or
  //packages.
  //This will not create the configureDirectory
  Executable(std::string targetName_, File file, Directory configureDirectory_);
};

void to_json(Json &j, const Executable &executable);

struct PVExecutable {//Package Variant Executable
  std::vector<IDD> includeDirectoryDependencies;
  std::vector<LibraryDependency> libraryDependencies;
  std::vector<CompilerFlagsDependency> compilerFlagsDependencies;
  std::vector<LinkerFlagsDependency> linkerFlagsDependencies;
  std::vector<CompileDefinitionDependency> compileDefinitionDependencies;
  std::vector<File> sourceFiles;
  std::string targetName;
  Directory configureDirectory;
  //todo: Why is it not Directory. Rename it to outputDirectoryPath
  fs::path buildDirectoryPath;
  //configureDirectory will be same as project::SOURCE_DIRECTORY. And the executable's build directory will be
  //same as configureDirectory. To specify a different build directory, set the buildDirectoryPath variable.
  Executable(std::string targetName_, File file);

  //This will create a configure directory under the project::BUILD_DIRECTORY.
  Executable(std::string targetName_, File file, fs::path configureDirectoryPathRelativeToProjectBuildPath);

  //TODO: This constructor should not exist. A Project's configure directory
  //should also be the configure directory for all of the targets or
  //packages.
  //This will not create the configureDirectory
  Executable(std::string targetName_, File file, Directory configureDirectory_);
};

void to_json(Json &j, const Executable &executable);
#endif// EXECUTABLE_HPP
