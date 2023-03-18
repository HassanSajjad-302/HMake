#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);

    DSC<CppSourceTarget> &catStatic = GetCppStaticDSC("Cat-Static");
    catStatic.getSourceTarget()
        .SOURCE_FILES("Cat/src/Cat.cpp")
        .PUBLIC_INCLUDES("Cat/header")
        .PRIVATE_COMPILE_DEFINITION("CAT_EXPORT", "");

    DSC<CppSourceTarget> &animalStatic = GetCppExeDSC("Animal-Static");
    animalStatic.PRIVATE_LIBRARIES(&catStatic);
    animalStatic.getSourceTarget().SOURCE_FILES("main.cpp").PRIVATE_COMPILE_DEFINITION("CAT_EXPORT", "");

    DSC<CppSourceTarget> &catShared = GetCppSharedDSC("Cat-Shared");
    catShared.getSourceTarget()
        .SOURCE_FILES("Cat/src/Cat.cpp")
        .PUBLIC_INCLUDES("Cat/header")
        .PRIVATE_COMPILE_DEFINITION("CAT_EXPORT", "__declspec(dllexport)");

    DSC<CppSourceTarget> &animalShared = GetCppExeDSC("Animal-Shared");
    animalShared.PRIVATE_LIBRARIES(&catShared);
    animalShared.getSourceTarget()
        .SOURCE_FILES("main.cpp")
        .PRIVATE_COMPILE_DEFINITION("CAT_EXPORT", "__declspec(dllimport)");

    configureOrBuild();
}