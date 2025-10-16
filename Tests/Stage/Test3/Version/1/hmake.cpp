#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    config.assign(config.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b);

    DSC<CppSourceTarget> &lib4 = config.getCppStaticDSC("lib4");
    lib4.getSourceTarget()
        .moduleDirsRE("lib4/private", ".*cpp")
        .privateIncludesRE("lib4/private", ".*hpp")
        .publicIncludes("lib4/public");

    DSC<CppSourceTarget> &lib3 = config.getCppStaticDSC("lib3").publicDeps(lib4);
    lib3.getSourceTarget()
        .moduleDirsRE("lib3/private", ".*cpp")
        .privateIncludesRE("lib3/private", ".*hpp")
        .publicHUIncludes("lib3/public");

    DSC<CppSourceTarget> &lib2 = config.getCppStaticDSC("lib2").privateDeps(lib3);
    lib2.getSourceTarget()
        .moduleDirsRE("lib2/private", ".*cpp")
        .privateIncludesRE("lib2/private", ".*hpp")
        .publicIncludes("lib2/public");

    DSC<CppSourceTarget> &lib1 = config.getCppStaticDSC("lib1").publicDeps(lib2);
    lib1.getSourceTarget()
        .moduleDirsRE("lib1/private", ".*cpp")
        .privateIncludesRE("lib1/private", ".*hpp")
        .publicIncludes("lib1/public");

    config.getCppExeDSC("app").privateDeps(lib1).getSourceTarget().moduleFiles("main.cpp");
}

void buildSpecification()
{
    getConfiguration("Debug").assign(TreatModuleAsSource::NO, ConfigType::DEBUG);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
