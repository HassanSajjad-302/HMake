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

struct executable{
    std::vector<IDD> includeDirectoryDependencies;
    std::vector<libraryDependency> libraryDependencies;
    std::vector<compilerOptionDependency> compilerOptionDependencies;
    std::vector<compileDefinitionDependency> compileDefinitionDependencies;
    std::vector<file> sourceFiles;
    std::string targetName;

    bool runTimeDirectoryEnabled = false;
    directory runTimeDir;
    executable(std::string targetName, file file);
    void setRunTimeOutputDirectory(directory dir);
    std::string getScript();
};

#endif // EXECUTABLE_HPP
