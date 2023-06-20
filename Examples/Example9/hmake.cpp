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

void configurationSpecification(Configuration &config)
{
    config.compilerFeatures.PRIVATE_INCLUDES("include/");

    DSC<CppSourceTarget> &stdhu = config.GetCppObjectDSC("stdhu");

    stdhu.getSourceTargetPointer()->setModuleScope().assignStandardIncludesToHUIncludes();
    config.moduleScope = stdhu.getSourceTargetPointer();

    DSC<CppSourceTarget> &lib4 = config.GetCppTargetDSC("lib4", config.targetType);
    DSC<CppSourceTarget> &lib3 = config.GetCppTargetDSC("lib3", config.targetType).PUBLIC_LIBRARIES(&lib4);
    DSC<CppSourceTarget> &lib2 = config.GetCppTargetDSC("lib2", config.targetType).PRIVATE_LIBRARIES(&lib3);
    DSC<CppSourceTarget> &lib1 = config.GetCppTargetDSC("lib1", config.targetType).PUBLIC_LIBRARIES(&lib2);
    DSC<CppSourceTarget> &app = config.GetCppExeDSC("app").PRIVATE_LIBRARIES(&lib1, &stdhu);

    initializeTargets(lib1, lib2, lib3, lib4, app);
}

void buildSpecification()
{
    CxxSTD cxxStd = toolsCache.vsTools[0].compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_23;

    Configuration &static_ = GetConfiguration("static");
    static_.ASSIGN(cxxStd, TreatModuleAsSource::NO, ConfigType::DEBUG, TargetType::LIBRARY_STATIC);
    configurationSpecification(static_);

    Configuration &object = GetConfiguration("object");
    object.ASSIGN(cxxStd, TreatModuleAsSource::NO, ConfigType::DEBUG, TargetType::LIBRARY_OBJECT);
    configurationSpecification(object);
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