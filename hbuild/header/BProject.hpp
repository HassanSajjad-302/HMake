

#ifndef HMAKE_HBUILD_HEADER_BPROJECT_HPP
#define HMAKE_HBUILD_HEADER_BPROJECT_HPP

#include "Library.hpp"
#include "filesystem"
namespace fs = std::filesystem;
class BProject {
public:
  static inline fs::path projectFilePath;
  static inline fs::path compilerPath;
  static inline fs::path linkerPath;
  static inline std::string compilerFlags;
  static inline std::string linkerFlags;
  static inline LibraryType libraryType;
  static void parseProjectFileJsonAndInitializeBProjectStaticVariables(Json &projectFileJson);
};

#endif//HMAKE_HBUILD_HEADER_BPROJECT_HPP
