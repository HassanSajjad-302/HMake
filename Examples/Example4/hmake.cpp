#include "Configure.hpp"

void buildSpecification()
{
    DSC<CppSourceTarget> &catStatic = GetCppStaticDSC("Cat-Static", true, "CAT_EXPORT");
    catStatic.getSourceTarget().SOURCE_FILES("Cat/src/Cat.cpp").PUBLIC_INCLUDES("Cat/header");

    GetCppExeDSC("Animal-Static").PRIVATE_LIBRARIES(&catStatic).getSourceTarget().SOURCE_FILES("main.cpp");

    DSC<CppSourceTarget> &catShared = GetCppSharedDSC("Cat-Shared", true, "CAT_EXPORT");
    catShared.getSourceTarget().SOURCE_FILES("Cat/src/Cat.cpp").PUBLIC_INCLUDES("Cat/header");

    GetCppExeDSC("Animal-Shared").PRIVATE_LIBRARIES(&catShared).getSourceTarget().SOURCE_FILES("main.cpp");
}

MAIN_FUNCTION