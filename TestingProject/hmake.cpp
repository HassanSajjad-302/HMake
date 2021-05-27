
#include "Configure.hpp"
#include "Executable.hpp"
#include "Initialize.hpp"
#include "Project.hpp"
int main(int argc, const char **argv) {
  initialize(argc, argv);

  Project animal("Animal");

  Executable animalExe{"Animal", File("main.cpp")};

  Library cat("Cat");
  IDD catIncludeDependency{Directory("Cat/header"), DependencyType::PUBLIC};
  cat.includeDirectoryDependencies.push_back(catIncludeDependency);
  cat.sourceFiles.emplace_back("Cat/src/Cat.cpp");

  Library kitten("Kitten");
  IDD kittenIncludeDependency{Directory("Kitten/header"), DependencyType::PUBLIC};
  kitten.includeDirectoryDependencies.push_back(kittenIncludeDependency);
  kitten.sourceFiles.emplace_back(File("Kitten/src/Kitten.cpp"));

  cat.libraryDependencies.emplace_back(kitten, DependencyType::PUBLIC);
  animalExe.libraryDependencies.emplace_back(cat, DependencyType::PRIVATE);
  Project::projectExecutables.push_back(animalExe);

  configure(animal);
}
