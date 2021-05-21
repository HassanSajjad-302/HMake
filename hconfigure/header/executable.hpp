#ifndef EXECUTABLE_HPP
#define EXECUTABLE_HPP

#include<string>
#include<filesystem>
#include<vector>
#include"cxx_standard.hpp"
#include"directory.hpp"
#include"file.hpp"
#include "IDD.hpp"
#include "libraryDependency.hpp"
#include "nlohmann/json.hpp"

struct executable{
    std::vector<IDD> includeDirectoryDependencies;
    std::vector<libraryDependency> libraryDependencies;
    std::vector<compilerFlagDependency> compilerFlagDependencies;
    std::vector<compileDefinitionDependency> compileDefinitionDependencies;
    std::vector<file> sourceFiles;
    std::string targetName;
    directory configureDirectory;
    fs::path buildDirectoryPath;
    //configureDirectory will be same as project::SOURCE_DIRECTORY. And the executable's build directory will be
    //same as configureDirectory. To specify a different build directory, set the buildDirectoryPath variable.
    executable(std::string targetName_, file file);

    //This will create a configure directory under the project::BUILD_DIRECTORY.
    executable(std::string targetName_, file file, fs::path configureDirectoryPathRelativeToProjectBuildPath);

    //This will not create the configureDirectory
    executable(std::string targetName_, file file, directory configureDirectory_);
};

void to_json(json &j, const executable &p);
#endif // EXECUTABLE_HPP
