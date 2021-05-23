

#ifndef HMAKE_BINARYDIREXPORTFILE_HPP
#define HMAKE_BINARYDIREXPORTFILE_HPP

#include "Library.hpp"
#include <string>
#include <vector>

struct BinaryDirExportFile {
  std::string fileName;
  std::string namespaceName;
  std::vector<Library> exportingLibraries;
  BinaryDirExportFile(std::string fileName_, std::vector<Library> exportingLibraries_,
                      std::string namespaceName_ = "");
};

#endif//HMAKE_BINARYDIREXPORTFILE_HPP
