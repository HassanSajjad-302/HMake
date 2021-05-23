

#ifndef HMAKE_COMPILEDEFINITIONDEPENDENCY_HPP
#define HMAKE_COMPILEDEFINITIONDEPENDENCY_HPP

#include "DependencyType.hpp"
#include <string>

struct CompileDefinitionDependency {
  std::string compileDefinition;
  DependencyType compileDefinitionDependency;
};

#endif//HMAKE_COMPILEDEFINITIONDEPENDENCY_HPP
