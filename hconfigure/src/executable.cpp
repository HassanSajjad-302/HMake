#include "executable.hpp"
#include "project.hpp"

executable::executable(std::string targetName_, file file):
        targetName(std::move(targetName_)), configureDirectory(project::BUILD_DIRECTORY.path),
        buildDirectoryPath(project::BUILD_DIRECTORY.path){
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

//This will imply that directory already exists. While in the above constructor directory will be built while building
//the project.
executable::executable(std::string targetName_, file file, directory configureDirectory_):
        targetName(std::move(targetName_)), buildDirectoryPath(configureDirectory_.path),
        configureDirectory(std::move(configureDirectory_)) {
    sourceFiles.emplace_back(std::move(file));
}

void to_json(json &j, const executable &p) {
    j["PROJECT_FILE_PATH"] = project::BUILD_DIRECTORY.path.string() + project::PROJECT_NAME + ".hmake";
    j["NAME"] = p.targetName;
    j["BUILD_DIRECTORY"] = p.buildDirectoryPath.string();
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

    json compilerFlagsArray;
    for(auto e: p.compilerFlagDependencies){
        json compilerFlagObject;
        compilerFlagObject["VALUE"] = e.compilerOption;
        compilerFlagsArray.push_back(compilerFlagObject);
    }
    j["COMPILER_TRANSITIVE_FLAGS"] = compilerFlagsArray;

    json compileDefinitionsArray;
    for(auto e: p.compileDefinitionDependencies){
        json compileDefinitionObject;
        compileDefinitionObject["NAME"] = e.compileDefinition;
        compileDefinitionsArray.push_back(compileDefinitionObject);
    }
    j["COMPILE_DEFINITIONS"] = compileDefinitionsArray;
}
