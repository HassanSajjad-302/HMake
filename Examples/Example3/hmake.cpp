#include "Configure.hpp"

int main()
{
    Cache::initializeCache();
    Project project;
    ProjectVariant variantRelease;

    Executable app("app", variantRelease);
    // Just the files matching the regex in second argument will be used for compilation. i.e. hmake.cpp will
    // be ignored. Please not it also checks the subdirectories. More control over this is planned.
    ADD_SRC_DIR_TO_TARGET(app, ".", "file[1-4]\\.cpp|main\\.cpp");
    // ADD_SRC_DIR_TO_TARGET(animal, ".", "\\w+\\b(?<!\\bhmake)");

    ADD_EXECUTABLES_TO_VARIANT(variantRelease, app);
    project.projectVariants.push_back(variantRelease);

    ProjectVariant variantDebug;
    variantDebug.configurationType = ConfigType::DEBUG;

    app.assignDifferentVariant(variantDebug);
    ADD_EXECUTABLES_TO_VARIANT(variantDebug, app);
    project.projectVariants.push_back(variantDebug);
    project.configure();

    // Target has other members/features which I will not demonstrate. Such as
    //  compilerFlagsDependencies
    //  linkerFlagsDependencies
    //  outputName (This is name of the compiled target binary. Currently, not a lot of customization is
    // available for it. i.e. You may want to use version with name or not. Currently, for a library
    // the name will always be lib + targetName + .a.)
    //  outputDirectory(This is where binary will be compiled. You can't change the configureDirectory, however
    //  you can change the outputDirectory).
}