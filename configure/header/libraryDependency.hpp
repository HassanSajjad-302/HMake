
#ifndef HMAKE_LIBRARYDEPENDENCY_HPP
#define HMAKE_LIBRARYDEPENDENCY_HPP

#include "library.hpp"
#include "dependencyType.hpp"

struct libraryDependency{
library library_;
dependencyType libraryDependencyType;
libraryDependency(library library1, dependencyType dependency_ = dependencyType::PUBLIC);
};
#endif //HMAKE_LIBRARYDEPENDENCY_HPP
