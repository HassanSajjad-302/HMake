#ifndef HMAKE_LIBRARY_HPP
#define HMAKE_LIBRARY_HPP

#include<string>
#include<filesystem>
#include<vector>
#include"cxx_standard.hpp"
#include "compileDefinitionDependency.hpp"
#include "compilerOptionsDependency.hpp"
#include"IDD.hpp"
#include"file.hpp"

enum class libraryType{
    STATIC, SHARED, MODULE
};

struct libraryDependency;
struct library {
    std::vector<IDD> includeDirectoryDependencies;
    std::vector<libraryDependency> libraryDependencies;
    std::vector<compilerFlagDependency> compilerOptionDependencies;
    std::vector<compileDefinitionDependency> compileDefinitionDependencies;
    std::vector<file> sourceFiles;
    std::string targetName;
    libraryType type;

    library(std::string targetName, libraryType type_);
    std::string getScript();
};


#endif //HMAKE_LIBRARY_HPP
