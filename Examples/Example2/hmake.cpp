#include "Configure.hpp"

void buildSpecification()
{
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
        .PRIVATE_COMPILE_DEFINITION("CAT_EXPORT", "");

    DSC<CppSourceTarget> &animalShared = GetCppExeDSC("Animal-Shared");
    animalShared.PRIVATE_LIBRARIES(&catShared);
    animalShared.getSourceTarget().SOURCE_FILES("main.cpp").PRIVATE_COMPILE_DEFINITION("CAT_EXPORT", "");
}

#ifdef EXE
int main(int argc, char **argv)
{
    try
    {
        initializeCache(getBuildSystemModeFromArguments(argc, argv));
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#else
extern "C" EXPORT int func2(BSMode bsMode_)
{
    try
    {
        exportAllSymbolsAndInitializeGlobals();
        initializeCache(bsMode_);
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif