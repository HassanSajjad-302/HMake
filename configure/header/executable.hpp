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
    std::vector<compilerOptionDependency> compilerOptionDependencies;
    std::vector<compileDefinitionDependency> compileDefinitionDependencies;
    std::vector<file> sourceFiles;
    std::string targetName;
    directory sourceDirectory;
    directory configureDirectory;
    directory buildDirectory;
    executable(std::string targetName, file file);
    executable(std::string targetName_, directory sourceDirectory_);
    executable(std::string targetName_, directory sourceDirectory_, directory configureDirectory_);
    executable(std::string targetName_, directory sourceDirectory_, directory configureDirectory_, directory buildDirectory_);
};

typedef nlohmann::json json;

void to_json(json &j, const executable &p);
#endif // EXECUTABLE_HPP
