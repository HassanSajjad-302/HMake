#include "Configure.hpp"

int main()
{
    Cache::initializeCache();
    Project project;
    ProjectVariant variantRelease;

    Executable animal("Animal", variantRelease);
    ADD_SRC_FILES_TO_TARGET(animal, "main.cpp");

    Library cat("Cat", variantRelease);
    ADD_SRC_FILES_TO_TARGET(cat, "Cat/src/Cat.cpp");
    ADD_PUBLIC_IDDS_TO_TARGET(cat, "Cat/header");
    ADD_PRIVATE_LIB_DEPS_TO_TARGET(animal, cat);
    ADD_EXECUTABLES_TO_VARIANT(variantRelease, animal);

    ProjectVariant variantDebug;
    variantDebug.configurationType = ConfigType::DEBUG;
    // Once this function is called then animal and it's library dependencies will have properties of
    // variantDebug i.e. their configurationType will be ConfigType::DEBUG.
    animal.assignDifferentVariant(variantDebug);
    ADD_EXECUTABLES_TO_VARIANT(variantDebug, animal);

    project.projectVariants.push_back(variantRelease);
    project.projectVariants.push_back(variantDebug);
    project.configure();

    // I suggest you see the directory structure of your buildDir.
}