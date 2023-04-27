#include "Configure.hpp"

void buildSpecification()
{
    Configuration &debug = GetConfiguration("Release");

    CxxSTD cxxStd = debug.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b;

    debug.ASSIGN(cxxStd, TreatModuleAsSource::YES, ConfigType::RELEASE);

    // configuration.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");

    auto configureFunc = [](Configuration &configuration) {
        DSC<CppSourceTarget> &lib4 = configuration.GetCppStaticDSC("lib4");
        lib4.getSourceTarget().SOURCE_DIRECTORIES("lib4/private/", ".*cpp").PUBLIC_INCLUDES("lib4/public/");

        DSC<CppSourceTarget> &lib3 = configuration.GetCppStaticDSC("lib3").PUBLIC_LIBRARIES(&lib4);
        lib3.getSourceTarget().SOURCE_DIRECTORIES("lib3/private/", ".*cpp").PUBLIC_INCLUDES("lib3/public/");

        DSC<CppSourceTarget> &lib2 = configuration.GetCppStaticDSC("lib2").PRIVATE_LIBRARIES(&lib3);
        lib2.getSourceTarget().SOURCE_DIRECTORIES("lib2/private/", ".*cpp").PUBLIC_INCLUDES("lib2/public/");

        DSC<CppSourceTarget> &lib1 = configuration.GetCppStaticDSC("lib1").PUBLIC_LIBRARIES(&lib2);
        lib1.getSourceTarget().SOURCE_DIRECTORIES("lib1/private/", ".*cpp").PUBLIC_INCLUDES("lib1/public/");

        DSC<CppSourceTarget> &app = configuration.GetCppExeDSC("app").PRIVATE_LIBRARIES(&lib1);
        app.getSourceTarget().SOURCE_FILES("main.cpp");
    };

    configureFunc(debug);
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
