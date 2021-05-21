
#ifndef HMAKE_COMPILEROPTIONSDEPENDENCY_HPP
#define HMAKE_COMPILEROPTIONSDEPENDENCY_HPP

#include <string>
#include "dependencyType.hpp"

struct compilerFlagDependency {
    std::string compilerOption;
    dependencyType compilerOptionDependency;
};


#endif //HMAKE_COMPILEROPTIONSDEPENDENCY_HPP
