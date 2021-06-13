
#include "Configure.hpp"
int main(int argc, const char **argv) {
  initializeCacheAndInitializeProject(argc, argv, "Animal");

  Executable animalExe{"Animal"};

  Library cat("Cat");
  IDD catIncludeDependency{Directory("Cat/header"), DependencyType::PUBLIC};
  cat.includeDirectoryDependencies.push_back(catIncludeDependency);
  cat.sourceFiles.emplace_back("Cat/src/Cat.cpp");

  Library kitten("Kitten");
  IDD kittenIncludeDependency{Directory("Kitten/header"), DependencyType::PUBLIC};
  kitten.includeDirectoryDependencies.push_back(kittenIncludeDependency);
  kitten.sourceFiles.emplace_back(File("Kitten/src/Kitten.cpp"));

  cat.libraryDependencies.emplace_back(LibraryDependency{kitten, DependencyType::PRIVATE});
  animalExe.libraryDependencies.emplace_back(LibraryDependency{cat, DependencyType::PRIVATE});
  Project::executables.push_back(animalExe);
  configure();

  Package package("AnimalPackage");
  PackageVariant variantRelease;
  variantRelease.configurationType = ConfigType::RELEASE;
  Json vJson;
  vJson["CONFIGURATION"] = "RELEASE";
  variantRelease.libraries.push_back(cat);
  variantRelease.json = vJson;
  package.packageVariants.push_back(variantRelease);

  PackageVariant variantDebug;
  variantDebug.configurationType = ConfigType::DEBUG;
  Json vJsonDebug;
  vJsonDebug["CONFIGURATION"] = "DEBUG";
  variantDebug.libraries.push_back(cat);
  variantDebug.json = vJsonDebug;
  package.packageVariants.push_back(variantDebug);
  package.configure();
}

/*SubDirectory cat2("./cat/");
cat2.configure();
SLibrary cat = cat2.getLibrary("cat"); //Subdirectory Library
PLibrary //Packaged Library*/