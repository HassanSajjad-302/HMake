#include "Configure.hpp"

int main() {
  Cache::initializeCache();

  ProjectVariant variantRelease;

  Executable pets("Pets", variantRelease);
  pets.sourceFiles.emplace_back("main.cpp");

  Library goat("Goat", variantRelease);
  goat.sourceFiles.emplace_back("Goat/src/Goat.cpp");
  goat.includeDirectoryDependencies.push_back(IDD{.includeDirectory = Directory("Goat/header"),
                                                  .dependencyType = DependencyType::PUBLIC});

  //argument is packagePath
  //ConsumePackage
  CPackage catAndDog("../Example7/Build/install/Pets");

  Json consumeVariantJson;

  consumeVariantJson["CONFIGURATION"] = "RELEASE";
  //PackagePrebuiltLibrary
  PPLibrary dogRelease("Dog", catAndDog, catAndDog.getVariant(consumeVariantJson));

  consumeVariantJson["CONFIGURATION"] = "DEBUG";
  //PackagedPrebuiltLibrary
  PPLibrary dogDebug("Dog", catAndDog, catAndDog.getVariant(consumeVariantJson));

  //PPLibrary has a variable named imported which is defaulted to true. If this library is included in a
  //package in this hmake file, it will be referred in the package but not copied. However if it is set to
  //false, it will be copied like normal library.

  goat.libraryDependencies.emplace_back(dogRelease, DependencyType::PUBLIC);
  pets.libraryDependencies.emplace_back(goat, DependencyType::PUBLIC);

  variantRelease.executables.emplace_back(pets);

  goat.libraryDependencies.clear();
  goat.configurationType = ConfigType::DEBUG;
  goat.libraryDependencies.emplace_back(dogDebug, DependencyType::PUBLIC);
  pets.libraryDependencies.clear();
  pets.configurationType = ConfigType::DEBUG;
  pets.libraryDependencies.emplace_back(goat, DependencyType::PUBLIC);

  ProjectVariant variantDebug;
  variantDebug.executables.emplace_back(pets);

  Project project;
  project.projectVariants.push_back(variantDebug);
  project.projectVariants.push_back(variantRelease);
  project.configure();

  //Note: hmake only support one file. So, if you want to include other source code. Either include it using hmake packages or
  //just add it directly in the file. This, IMHO, is a very good and very simple option. Even if size becomes too large it is
  //still manageable IMHO.
}