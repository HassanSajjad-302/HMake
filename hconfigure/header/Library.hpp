#ifndef HMAKE_LIBRARY_HPP
#define HMAKE_LIBRARY_HPP

#include "CompileDefinitionDependency.hpp"
#include "CompilerFlagsDependency.hpp"
#include "CxxStandard.hpp"
#include "File.hpp"
#include "IDD.hpp"
#include "LinkerFlagsDependency.hpp"
#include <filesystem>
#include <string>
#include <vector>

enum class LibraryType {
  STATIC,
  SHARED,
};

//todo: Add headeronly library dependency to the project.

struct LibraryDependency;
struct Library {
  std::vector<IDD> includeDirectoryDependencies;
  std::vector<LibraryDependency> libraryDependencies;
  std::vector<CompilerFlagsDependency> compilerFlagsDependencies;
  std::vector<LinkerFlagsDependency> linkerFlagsDependencies;
  std::vector<CompileDefinitionDependency> compileDefinitionDependencies;
  std::vector<File> sourceFiles;
  std::string targetName;
  Directory configureDirectory;
  fs::path buildDirectoryPath;
  LibraryType libraryType;

  Library(std::string targetName);
  Library(std::string targetName_, fs::path configureDirectoryPathRelativeToProjectBuildPath);
  Library(std::string targetName_, Directory configureDirectory_);
};

void to_json(Json &j, const LibraryType &p);
void to_json(Json &j, const Library &library);
#endif//HMAKE_LIBRARY_HPP
