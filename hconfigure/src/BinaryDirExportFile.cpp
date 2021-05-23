

#include "BinaryDirExportFile.hpp"
#include "LibraryDependency.hpp"
BinaryDirExportFile::BinaryDirExportFile(std::string fileName_, std::vector<Library> exportingLibraries_, std::string namespaceName_) : fileName(fileName_), exportingLibraries(exportingLibraries_),
                                                                                                                                        namespaceName(namespaceName_) {
}