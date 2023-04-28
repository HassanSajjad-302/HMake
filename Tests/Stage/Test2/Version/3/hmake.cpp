#include "Configure.hpp"

void buildSpecification()
{
    Configuration &debug = GetConfiguration("Debug");

    CxxSTD cxxStd = debug.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b;

    debug.ASSIGN(cxxStd, TreatModuleAsSource::YES, ConfigType::DEBUG);

    // configuration.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");

    auto configureFunc = [](Configuration &configuration) {
        DSC<CppSourceTarget> &app = configuration.GetCppExeDSC("app");
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
