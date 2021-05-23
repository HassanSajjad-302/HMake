
#ifndef HMAKE_BINARYDIRIMPORTFILE_HPP
#define HMAKE_BINARYDIRIMPORTFILE_HPP

#include "File.hpp"
class BinaryDirImportFile {
  File fileForImportingTargets;
  std::string namespceName;
  BinaryDirImportFile(File fileForImportingTargets_, std::string namespaceName_ = "");
};
#endif//HMAKE_BINARYDIRIMPORTFILE_HPP
