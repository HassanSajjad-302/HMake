#include "executable.hpp"


executable::executable(std::string targetName, file file){
this->targetName = targetName;
this->sourceFiles.emplace_back(std::move(file));
}

executable::executable(std::string targetName_, directory sourceDirectory_):targetName(std::move(targetName_)) {
    //TODO: check if source directory is under the project::SOURCE_DIRECTORY. Else throw the exception. Otherwise
    //initialize the required variables.

}

executable::executable(std::string targetName_, directory sourceDirectory_, directory configureDirectory_):
        targetName(std::move(targetName_)), sourceDirectory(std::move(sourceDirectory_)),
        configureDirectory(configureDirectory_),
        buildDirectory(std::move(configureDirectory_)){

}

executable::executable(std::string targetName_, directory sourceDirectory_, directory configureDirectory_, directory buildDirectory_):
        targetName(std::move(targetName_)), sourceDirectory(std::move(sourceDirectory_)),
        configureDirectory(std::move(configureDirectory_)),
        buildDirectory(std::move(buildDirectory_)){

}

void to_json(json &j, const executable &p) {
    j["NAME"] = p.targetName;
    json sourceFilesArray;
    for(auto e: p.sourceFiles){
        json sourceFileObject;
        sourceFileObject["PATH"] = e.path.string();
        sourceFilesArray.push_back(sourceFileObject);
    }
    j["SOURCE_FILES"] = sourceFilesArray;
    //library dependencies


    json IDDArray;
    for(auto e: p.includeDirectoryDependencies){
        json IDDObject;
        IDDObject["PATH"] = e.includeDirectory.path.string();
        IDDObject["TYPE"] = e.directoryDependency;
        IDDArray.push_back(IDDObject);
    }
    j["INCLUDE_DIRECTORIES"] = sourceFilesArray;


}
