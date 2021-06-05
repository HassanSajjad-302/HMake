

#ifndef HMAKE_HCONFIGURE_HEADER_CACHE_HPP
#define HMAKE_HCONFIGURE_HEADER_CACHE_HPP

#include "ConfigType.hpp"
#include "Library.hpp"
#include "filesystem"
#include "vector"
namespace fs = std::filesystem;
#include "Directory.hpp"
#include "Family.hpp"
struct Cache {
  static inline Directory SOURCE_DIRECTORY;
  static inline Directory CONFIGURE_DIRECTORY;
  static inline ConfigType projectConfigurationType;
  static inline std::vector<Compiler> compilerArray;
  static inline int selectedCompilerArrayIndex;
  static inline std::vector<Linker> linkerArray;
  static inline int selectedLinkerArrayIndex;
  static inline LibraryType libraryType;
  static inline Json cacheJson;
  static inline bool hasParent;
  static inline fs::path parentPath;
};

#endif//HMAKE_HCONFIGURE_HEADER_CACHE_HPP
