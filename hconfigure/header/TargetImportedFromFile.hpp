

#ifndef HMAKE_TARGETIMPORTEDFROMFILE_HPP
#define HMAKE_TARGETIMPORTEDFROMFILE_HPP

#include "BinaryDirImportFile.hpp"
struct TargetImportedFromFile {
  BinaryDirImportFile importingFile;
  std::string libraryName;
};

#endif//HMAKE_TARGETIMPORTEDFROMFILE_HPP
