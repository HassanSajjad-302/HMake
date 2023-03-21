#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);

    Cache::initializeCache();
    Project project;
    ProjectVariant variantRelease;

    DSC<CppSourceTarget> &animal = GetCppExeDSC("animal");
    animal.getSourceTarget().SOURCE_FILES("main.cpp");

    // PLibrary means prebuilt library. if you want to use a prebuilt library, you can use it this way.
    // Note that the library had to specify the includeDirectoryDependency explicitly. An easy was to use a packaged
    // library. Generating a hmake packaged library is shown in next example.
#ifdef _WIN32
    PLibrary cat("../Example2/Build/0/Cat/Cat.lib", LibraryType::STATIC);
#else
    PLibrary cat("../Example2/Build/0/Cat/libCat.a", LibraryType::STATIC);
#endif
    cat.includeDirectoryDependencies.emplace_back("../Example2/Cat/header/");
    animal.libraryDependencies.emplace_back(cat, DependencyType::PRIVATE);

    variantRelease.executables.push_back(animal);
    project.projectVariants.push_back(variantRelease);

    project.configure();
}