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
    DSC<CppSourceTarget> &stdhu = config.getCppObjectDSC("stdhu");

    stdhu.getSourceTargetPointer()->makeReqInclsUseable().interfaceIncludes("include");
    stdhu.getLinkOrArchiveTarget().usageRequirementLibraryDirectories =
        stdhu.getLinkOrArchiveTarget().requirementLibraryDirectories;

    config.compilerFeatures.reqIncls.clear();
    config.prebuiltBasicFeatures.requirementLibraryDirectories.clear();

    DSC<CppSourceTarget> &lib4 = config.getCppTargetDSC("lib4", config.targetType).privateLibraries(&stdhu);
    DSC<CppSourceTarget> &lib3 =
        config.getCppTargetDSC("lib3", config.targetType).publicLibraries(&lib4).privateLibraries(&stdhu);
    DSC<CppSourceTarget> &lib2 = config.getCppTargetDSC("lib2", config.targetType)
                                     .publicLibraries(&stdhu)
                                     .privateLibraries(&lib3)
                                     .privateLibraries(&stdhu);
    DSC<CppSourceTarget> &lib1 =
        config.getCppTargetDSC("lib1", config.targetType).publicLibraries(&lib2).privateLibraries(&stdhu);
    DSC<CppSourceTarget> &app = config.getCppExeDSC("app").privateLibraries(&lib1).privateLibraries(&stdhu);

    initializeTargets(&lib1, &lib2, &lib3, &lib4, &app);
}

void buildSpecification()
{
    CxxSTD cxxStd = toolsCache.vsTools[0].compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_23;

    Configuration &static_ = getConfiguration("static");
    static_.assign(cxxStd, TreatModuleAsSource::NO, ConfigType::DEBUG, TargetType::LIBRARY_STATIC);
    configurationSpecification(static_);

    Configuration &object = getConfiguration("object");
    object.assign(cxxStd, TreatModuleAsSource::NO, ConfigType::DEBUG, TargetType::LIBRARY_OBJECT);
    configurationSpecification(object);
}

MAIN_FUNCTION