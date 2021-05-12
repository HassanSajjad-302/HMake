

#ifndef HMAKE_TARGETIMPORTEDFROMFILE_HPP
#define HMAKE_TARGETIMPORTEDFROMFILE_HPP

#include "binaryDirImportFile.hpp"
struct targetImportedFromFile{
    binaryDirImportFile importingFile;
    std::string libraryName;
};


#endif //HMAKE_TARGETIMPORTEDFROMFILE_HPP
