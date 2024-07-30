#include "Configure.hpp"

void buildSpecification()
{
    DSC<CppSourceTarget> &catStatic = getCppStaticDSC("Cat-Static", true, "CAT_EXPORT");
    catStatic.getSourceTarget().sourceFiles("Cat/src/Cat.cpp").publicIncludes("Cat/header");

    getCppExeDSC("Animal-Static").privateLibraries(&catStatic).getSourceTarget().sourceFiles("main.cpp");

    DSC<CppSourceTarget> &catShared = getCppSharedDSC("Cat-Shared", true, "CAT_EXPORT");
    catShared.getSourceTarget().sourceFiles("Cat/src/Cat.cpp").publicIncludes("Cat/header");

    getCppExeDSC("Animal-Shared").privateLibraries(&catShared).getSourceTarget().sourceFiles("main.cpp");
}

MAIN_FUNCTION