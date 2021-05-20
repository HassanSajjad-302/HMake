#include "executable.hpp"
#include "project.hpp"

executable::executable(std::string targetName_, file file):
        targetName(std::move(targetName_)){
    sourceFiles.emplace_back(std::move(file));
}

executable::executable(std::string targetName_, file file, fs::path configureDirectoryPathRelativeToProjectBuildPath):
        targetName(std::move(targetName_)) {
    auto targetConfigureDirectoryPath =
            project::BUILD_DIRECTORY.path / configureDirectoryPathRelativeToProjectBuildPath;
    fs::create_directory(targetConfigureDirectoryPath);
    configureDirectory = directory(targetConfigureDirectoryPath);
    buildDirectoryPath = targetConfigureDirectoryPath;
    sourceFiles.emplace_back(std::move(file));
}

executable::executable(std::string targetName_, file file, directory configureDirectory_):
        targetName(std::move(targetName_)), buildDirectoryPath(configureDirectory_.path),
        configureDirectory(std::move(configureDirectory_)) {
    sourceFiles.emplace_back(std::move(file));
}

void to_json(json &j, const executable &p) {
    j["BUILD_DIRECTORY"] = project::BUILD_DIRECTORY.path.string();
    j["NAME"] = p.targetName;
    json sourceFilesArray;
    for(auto e: p.sourceFiles){
        sourceFilesArray.push_back(e.path.string());
    }
    j["SOURCE_FILES"] = sourceFilesArray;
    //library dependencies


    json IDDArray;
    for(auto e: p.includeDirectoryDependencies){
        json IDDObject;
        IDDObject["PATH"] = e.includeDirectory.path.string();
        IDDArray.push_back(IDDObject);
    }
    j["INCLUDE_DIRECTORIES"] = IDDArray;

    json compilerOptionsArray;
    for(auto e: p.compilerOptionDependencies){
        json compilerOptionObject;
        compilerOptionObject["VALUE"] = e.compilerOption;
        compilerOptionsArray.push_back(compilerOptionObject);
    }
    j["COMPILER_OPTIONS"] = compilerOptionsArray;

    json compileDefinitionsArray;
    for(auto e: p.compileDefinitionDependencies){
        json compileDefinitionObject;
        compileDefinitionObject["NAME"] = e.compileDefinition;
        compileDefinitionsArray.push_back(compileDefinitionObject);
    }
    j["COMPILE_DEFINITIONS"] = compileDefinitionsArray;
}
