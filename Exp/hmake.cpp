#include "Configure.hpp"

void configurationSpecification(Configuration &configuration)
{
    /* configuration.linkerFeatures.linker.bTPath =
         "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\Llvm\\bin\\lld-link.exe";*/
    DSC<CppSourceTarget> &lib = configuration.GetCppStaticDSC("lib");
    lib.getSourceTarget().SOURCE_FILES("lib/lib.cpp").PUBLIC_INCLUDES("lib");
    DSC<CppSourceTarget> &app = configuration.GetCppExeDSC("app");
    app.getSourceTarget().SOURCE_FILES("main.cpp");
    app.PRIVATE_LIBRARIES(&lib);
    lib.PRIVATE_LIBRARIES(&app);
}

void buildSpecification()
{
    for (int i = 0; i < 1; ++i)
    {
        Configuration &configuration = GetConfiguration(std::to_string(i));
    }

    if (equivalent(path(configureDir), std::filesystem::current_path()))
    {
        for (const Configuration &configuration : targets<Configuration>)
        {
            configurationSpecification(const_cast<Configuration &>(configuration));
        }
    }
    else
    {
        for (const Configuration &configuration : targets<Configuration>)
        {
            if (const_cast<Configuration &>(configuration).getSelectiveBuildChildDir())
            {
                configurationSpecification(const_cast<Configuration &>(configuration));
            }
        }
    }
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