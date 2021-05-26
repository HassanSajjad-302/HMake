
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
  cat.sourceFiles.emplace_back("Cat/src/func.cpp");
  animalExe.sourceFiles.emplace_back("Cat/src/func.cpp");
  animalExe.libraryDependencies.emplace_back(cat, DependencyType::PRIVATE);

  Project::projectExecutables.push_back(animalExe);

  configure(animal);
}
