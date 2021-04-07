
#ifndef HMAKE_IDD_HPP
#define HMAKE_IDD_HPP

#include "directory.hpp"
#include "dependencyType.hpp"

// include directory dependency
struct IDD{
    directory includeDirectory;
    dependencyType directoryDependency;
};
#endif //HMAKE_IDD_HPP
