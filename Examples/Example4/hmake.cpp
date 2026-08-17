#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    DSC<CppTarget> &catStatic = config.getCppStaticDSC("Cat-Static", true, "CAT_EXPORT");
    catStatic.getSourceTarget().sourceFiles("Cat/src/Cat.cpp").publicIncludes("Cat/header");

    config.getCppExeDSC("Animal-Static").privateDeps(catStatic).getSourceTarget().sourceFiles("main.cpp");

    DSC<CppTarget> &catShared = config.getCppSharedDSC("Cat-Shared", true, "CAT_EXPORT");
    catShared.getSourceTarget().sourceFiles("Cat/src/Cat.cpp").publicIncludes("Cat/header");

    config.getCppExeDSC("Animal-Shared").privateDeps(catShared).getSourceTarget().sourceFiles("main.cpp");

    // No Cat archive/shared library is created. Animal's DSC consumes Cat's object producer directly.
    DSC<CppTarget> &catObject = config.getCppObjectDSC("Cat-Object", true, "CAT_EXPORT");
    catObject.getSourceTarget().sourceFiles("Cat/src/Cat.cpp").publicIncludes("Cat/header");

    config.getCppExeDSC("Animal-Object").privateDeps(catObject).getSourceTarget().sourceFiles("main.cpp");
}

void buildSpecification()
{
    getConfiguration();
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
