#include "Configure.hpp"

template <typename... T> void initializeTargets(DSC<CppSourceTarget> &target, T &...targets)
{
    CppSourceTarget &t = target.getSourceTarget();
    string str = t.name.substr(0, t.name.size() - 4); // Removing -cpp from the name
    t.MODULE_DIRECTORIES_RG("src/" + str + "/", ".*cpp")
        .HU_DIRECTORIES("src/" + str + "/")
        .HU_DIRECTORIES("include/" + str + "/");

    if constexpr (sizeof...(targets))
    {
        initializeTargets(targets...);
    }
}

void configurationSpecification(Configuration &configuration)
{
    configuration.compilerFeatures.PRIVATE_INCLUDES("include/");

    DSC<CppSourceTarget> &stdhu = configuration.GetCppObjectDSC("stdhu");

    stdhu.getSourceTargetPointer()->setModuleScope().assignStandardIncludesToHUIncludes();
    configuration.moduleScope = stdhu.getSourceTargetPointer();

    DSC<CppSourceTarget> &lib4 = configuration.GetCppStaticDSC("lib4");
    DSC<CppSourceTarget> &lib3 = configuration.GetCppStaticDSC("lib3").PUBLIC_LIBRARIES(&lib4);
    DSC<CppSourceTarget> &lib2 = configuration.GetCppStaticDSC("lib2").PRIVATE_LIBRARIES(&lib3);
    DSC<CppSourceTarget> &lib1 = configuration.GetCppStaticDSC("lib1").PUBLIC_LIBRARIES(&lib2);
    DSC<CppSourceTarget> &app = configuration.GetCppExeDSC("app").PRIVATE_LIBRARIES(&lib1, &stdhu);

    initializeTargets(lib1, lib2, lib3, lib4, app);
}

void buildSpecification()
{
    Configuration &debug = GetConfiguration("Debug");

    CxxSTD cxxStd = debug.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_23;

    debug.ASSIGN(cxxStd, TreatModuleAsSource::NO, ConfigType::DEBUG);

    configurationSpecification(debug);
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