

#ifndef HMAKE_HCONFIGURE_HEADER_SUBDIRECTORY_HPP
#define HMAKE_HCONFIGURE_HEADER_SUBDIRECTORY_HPP

#include "CONFIG_TYPE.hpp"
#include "Directory.hpp"
#include "Family.hpp"
#include "Library.hpp"

struct SubDirectory {
  Directory sourceDirectory;
  Directory buildDirectory;
  CONFIG_TYPE projectConfigurationType;
  std::vector<Compiler> compilerArray;
  int selectedCompilerArrayIndex;
  std::vector<Linker> linkerArray;
  int selectedLinkerArrayIndex;
  LibraryType libraryType;
  Json cacheVariablesJson;

  explicit SubDirectory(const fs::path &subDirectorySourcePathRelativeToParentSourcePath);
  SubDirectory(Directory sourceDirectory_, Directory buildDirectory_);
  void configure();
};

#endif//HMAKE_HCONFIGURE_HEADER_SUBDIRECTORY_HPP
