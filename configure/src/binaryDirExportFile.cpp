

#include "binaryDirExportFile.hpp"
#include "libraryDependency.hpp"
binaryDirExportFile::binaryDirExportFile(std::string fileName_, std::vector<library>
        exportingLibraries_, std::string namespaceName_): fileName(fileName_),exportingLibraries(exportingLibraries_),
        namespaceName(namespaceName_){

}

std::string binaryDirExportFile::getScript() {
    std::string script = "";
    script += "export(TARGETS\n";
    for(auto l: exportingLibraries){
        script += l.targetName + "\n";
    }
    if(!namespaceName.empty()){
        script += "NAMESPACE " + namespaceName + "\n";
    }
    script += "FILE " + fileName;
    return script;
}
