

#ifndef HMAKE_COMPILEDEFINITIONDEPENDENCY_HPP
#define HMAKE_COMPILEDEFINITIONDEPENDENCY_HPP

#include <string>
#include "dependencyType.hpp"

struct compileDefinitionDependency {
    std::string compileDefinition;
    dependencyType compileDefinitionDependency;
};


#endif //HMAKE_COMPILEDEFINITIONDEPENDENCY_HPP
