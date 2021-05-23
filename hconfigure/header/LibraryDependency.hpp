
#ifndef HMAKE_LIBRARYDEPENDENCY_HPP
#define HMAKE_LIBRARYDEPENDENCY_HPP

#include "DependencyType.hpp"
#include "Library.hpp"

struct LibraryDependency {
  Library library;
  DependencyType libraryDependencyType;
  explicit LibraryDependency(Library library_, DependencyType dependency = DependencyType::PUBLIC);
};
#endif//HMAKE_LIBRARYDEPENDENCY_HPP
