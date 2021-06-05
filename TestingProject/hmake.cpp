
#include "Configure.hpp"
#include "Executable.hpp"
#include "Initialize.hpp"
#include "Package.hpp"
#include "Project.hpp"
#include "SubDirectory.hpp"
int main(int argc, const char **argv) {
  initializeCacheAndInitializeProject(argc, argv, "Animal");

  Executable animalExe{"Animal", File("main.cpp")};

  SubDirectory cat2("./cat/");
  cat2.configure();
  SLibrary cat = cat2.getLibrary("cat"); //Subdirectory Library
  PLibrary //Packaged Library
  Library cat("Cat");
  IDD catIncludeDependency{Directory("Cat/header"), DependencyType::PUBLIC};
  cat.includeDirectoryDependencies.push_back(catIncludeDependency);
  cat.sourceFiles.emplace_back("Cat/src/Cat.cpp");

  Library kitten("Kitten");
  IDD kittenIncludeDependency{Directory("Kitten/header"), DependencyType::PUBLIC};
  kitten.includeDirectoryDependencies.push_back(kittenIncludeDependency);
  kitten.sourceFiles.emplace_back(File("Kitten/src/Kitten.cpp"));

  cat.libraryDependencies.emplace_back(kitten, DependencyType::PUBLIC);
  animalExe.libraryDependencieks.emplace_back(cat, DependencyType::PRIVATE);
  Project::projectExecutables.push_back(animalExe);
  configure();

  Package package("AnimalPackage");
  for (auto i : ConfigTypeIterator()) {
    Json j;
    j["CONFIGURATION"] = i;
    PackageVariant variant;
    variant.packageVariantConfigurationType = i;
    variant.associtedJson = j;
    package.packageVariants.emplace_back(variant);
  }
}
