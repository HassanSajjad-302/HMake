#include "Configure.hpp"

int main()
{
    Cache::initializeCache();
    Project project;
    ProjectVariant variantRelease(project);

    Library cat("Cat", variantRelease);
    cat.SOURCE_FILES("Cat/src/Cat.cpp").PUBLIC_INCLUDES("Cat/header");

    Executable animal("Animal", variantRelease);
    animal.SOURCE_FILES("main.cpp").PUBLIC_LIBRARIES(&cat);

    ProjectVariant variantDebug(project);
    variantDebug.configurationType = ConfigType::DEBUG;
    // Once this function is called then animal, and it's library dependencies will have properties of
    // variantDebug i.e. their configurationType will be ConfigType::DEBUG.
    variantDebug.copyAllTargetsFromOtherVariant(variantRelease);
    project.configure();

    // I suggest you see the directory structure of your buildDir.
}