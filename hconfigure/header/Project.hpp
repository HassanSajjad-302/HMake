
#ifndef HMAKE_PROJECT_HPP
#define HMAKE_PROJECT_HPP

#include "CONFIG_TYPE.hpp"
#include "Executable.hpp"
#include "Flags.hpp"
#include "Version.hpp"
#include "set"
#include "string"

struct Project {
  static inline std::string PROJECT_NAME;
  static inline Version PROJECT_VERSION;
  static inline Directory SOURCE_DIRECTORY;
  static inline Directory BUILD_DIRECTORY;
  static inline CONFIG_TYPE projectConfigurationType;
  static inline Compiler ourCompiler;
  static inline Linker ourLinker;
  static inline std::vector<Executable> projectExecutables;
  static inline std::vector<Library> projectLibraries;
  static inline Flags flags;
  static inline LibraryType libraryType;
  static inline bool hasParent;
  static inline fs::path parentPath;
};

void to_json(Json &j, const CONFIG_TYPE &p);
void to_json(Json &j, const Project &p);

#endif//HMAKE_PROJECT_HPP
