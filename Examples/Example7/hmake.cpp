#include "Configure.hpp"

void buildSpecification()
{
    GetCppExeDSC("app").getSourceTarget().MODULE_FILES("main.cpp", "std.cpp").setModuleScope();
    GetCppExeDSC("app2")
        .getSourceTarget()
        .MODULE_FILES("main2.cpp")
        .setModuleScope()
        .assignStandardIncludesToHUIncludes();
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