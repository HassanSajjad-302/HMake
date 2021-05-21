

#ifndef HMAKE_BINARYDIREXPORTFILE_HPP
#define HMAKE_BINARYDIREXPORTFILE_HPP

#include<string>
#include<vector>
#include "library.hpp"

struct binaryDirExportFile {
    std::string fileName;
    std::string namespaceName;
    std::vector<library> exportingLibraries;
    binaryDirExportFile(std::string fileName_, std::vector<library> exportingLibraries_,
                    std::string namespaceName_ = "");
    std::string getScript();
};


#endif //HMAKE_BINARYDIREXPORTFILE_HPP
