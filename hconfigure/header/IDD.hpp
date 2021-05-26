
#ifndef HMAKE_IDD_HPP
#define HMAKE_IDD_HPP

#include "DependencyType.hpp"
#include "Directory.hpp"

// include directory dependency
struct IDD {
  Directory includeDirectory;
  DependencyType dependencyType;
};
#endif//HMAKE_IDD_HPP
