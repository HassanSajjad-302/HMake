

#include <LibraryDependency.hpp>
#include <utility>

LibraryDependency::LibraryDependency(Library library_, DependencyType dependency) : library(std::move(library_)),
                                                                                    libraryDependencyType(dependency) {
}
