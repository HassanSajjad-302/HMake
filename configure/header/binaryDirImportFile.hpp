
#ifndef HMAKE_BINARYDIRIMPORTFILE_HPP
#define HMAKE_BINARYDIRIMPORTFILE_HPP

#include "file.hpp"
class binaryDirImportFile{
    file fileForImportingTargets;
    std::string namespceName;
    binaryDirImportFile(file fileForImportingTargets_, std::string namespaceName_ = "");
};
#endif //HMAKE_BINARYDIRIMPORTFILE_HPP
