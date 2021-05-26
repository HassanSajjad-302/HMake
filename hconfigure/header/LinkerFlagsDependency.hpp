
#ifndef HMAKE_HCONFIGURE_HEADER_LINKERFLAGSDEPENDENCY_HPP
#define HMAKE_HCONFIGURE_HEADER_LINKERFLAGSDEPENDENCY_HPP

#include "DependencyType.hpp"
#include <string>

struct LinkerFlagsDependency {
  std::string linkerFlags;
  DependencyType dependencyType;
};

#endif//HMAKE_HCONFIGURE_HEADER_LINKERFLAGSDEPENDENCY_HPP
