

#ifndef HMAKE_COMPILEDEFINITIONDEPENDENCY_HPP
#define HMAKE_COMPILEDEFINITIONDEPENDENCY_HPP

#include "DependencyType.hpp"
#include <string>

struct CompileDefinition {
  std::string name;
  std::string value;
};

struct CompileDefinitionDependency {
  CompileDefinition compileDefinition;
  DependencyType dependencyType;
};

#endif//HMAKE_COMPILEDEFINITIONDEPENDENCY_HPP
