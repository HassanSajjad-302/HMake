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