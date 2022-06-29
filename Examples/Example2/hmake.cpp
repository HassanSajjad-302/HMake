#include "Configure.hpp"

int main()
{
    Cache::initializeCache();
    Project project;
    ProjectVariant variantRelease;

    Executable animal("Animal", variantRelease);
    animal.sourceFiles.emplace("main.cpp");

    Library cat("Cat", variantRelease);
    cat.sourceFiles.emplace("Cat/src/Cat.cpp");
    IDD idd;
    idd.includeDirectory = Directory("Cat/header");
    idd.dependencyType = DependencyType::PUBLIC;
    cat.includeDirectoryDependencies.push_back(idd);
    animal.libraryDependencies.emplace_back(cat, DependencyType::PRIVATE);

    variantRelease.executables.push_back(animal);
    project.projectVariants.push_back(variantRelease);

    ProjectVariant variantDebug;
    variantDebug.configurationType = ConfigType::DEBUG;
    // Once this function is called then animal and it's library dependencies will have properties of
    // variantDebug i.e. their configurationType will be ConfigType::DEBUG.
    animal.assignDifferentVariant(variantDebug);
    variantDebug.executables.push_back(animal);
    project.projectVariants.push_back(variantDebug);
    project.configure();

    // I suggest you see the directory structure of your buildDir.
}