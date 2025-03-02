#include "Configure.hpp"

template <typename... T> void initializeTargets(DSC<CppSourceTarget> *target, T... targets)
{
    CppSourceTarget &t = target->getSourceTarget();
    string str = removeDashCppFromName(getLastNameAfterSlash(t.name));
    t.moduleDirectoriesRE("src/" + str + "/", ".*cpp")
        .privateHUDirectories("src/" + str)
        .publicHUDirectories("include/" + str);

    if constexpr (sizeof...(targets))
    {
        initializeTargets(targets...);
    }
}

void configurationSpecification(Configuration &config)
{
    config.stdCppTarget->getSourceTarget().interfaceIncludes("include");
    DSC<CppSourceTarget> &lib4 = config.getCppTargetDSC("lib4");
    DSC<CppSourceTarget> &lib3 = config.getCppTargetDSC("lib3").publicLibraries(&lib4);
    DSC<CppSourceTarget> &lib2 = config.getCppTargetDSC("lib2").privateLibraries(&lib3);
    DSC<CppSourceTarget> &lib1 = config.getCppTargetDSC("lib1").publicLibraries(&lib2);
    DSC<CppSourceTarget> &app = config.getCppExeDSC("app").privateLibraries(&lib1);

    initializeTargets(&lib1, &lib2, &lib3, &lib4, &app);
}

void buildSpecification()
{
    CxxSTD cxxStd = toolsCache.vsTools[0].compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_23;

    getConfiguration("static").assign(cxxStd, TreatModuleAsSource::NO, ConfigType::DEBUG, TargetType::LIBRARY_STATIC);
    getConfiguration("object").assign(cxxStd, TreatModuleAsSource::NO, ConfigType::DEBUG, TargetType::LIBRARY_OBJECT);
    callConfigurationSpecification();
}

MAIN_FUNCTION