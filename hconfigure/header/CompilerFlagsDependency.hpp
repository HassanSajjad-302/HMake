
#ifndef HMAKE_COMPILEROPTIONSDEPENDENCY_HPP
#define HMAKE_COMPILEROPTIONSDEPENDENCY_HPP

#include "DependencyType.hpp"
#include <string>

struct CompilerFlagsDependency {
  std::string compilerFlags;
  DependencyType compilerFlagsDependency;
};

#endif//HMAKE_COMPILEROPTIONSDEPENDENCY_HPP
