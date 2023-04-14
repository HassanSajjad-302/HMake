
#include "Configure.hpp"

void configurationSpecification(Configuration &configuration)
{
    configuration.GetCppExeDSC("app").getSourceTarget().SOURCE_DIRECTORIES(".", "file[1-4]\\.cpp|main\\.cpp");
}

void buildSpecification()
{
    Configuration &debug = GetConfiguration("Debug");
    debug.ASSIGN(ConfigType::DEBUG);
    Configuration &release = GetConfiguration("Release");
    release.ASSIGN(LTO::ON);

    configurationSpecification(debug);
    configurationSpecification(release);
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