#include "Configure.hpp"

void configurationSpecification(Configuration &debug)
{
    debug.assign(debug.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b);
    DSC<CppSourceTarget> &lib4 = debug.getCppStaticDSC("lib4");
    lib4.getSourceTarget()
        .sourceDirsRE("lib4/private", ".*cpp")
        .publicIncludes("lib4/public")
        .privateIncludes("lib4/private");

    DSC<CppSourceTarget> &lib3 = debug.getCppStaticDSC("lib3").publicDeps(lib4);
    lib3.getSourceTarget().moduleDirsRE("lib3/private", ".*cpp").publicHUIncludes("lib3/public");

    DSC<CppSourceTarget> &lib2 = debug.getCppStaticDSC("lib2").privateDeps(lib3);
    lib2.getSourceTarget().sourceDirsRE("lib2/private", ".*cpp").publicIncludes("lib2/public");

    DSC<CppSourceTarget> &lib1 = debug.getCppStaticDSC("lib1").publicDeps(lib2);
    lib1.getSourceTarget().sourceDirsRE("lib1/private", ".*cpp").publicIncludes("lib1/public");

    debug.getCppExeDSC("app").privateDeps(lib1).getSourceTarget().sourceFiles("main.cpp");
}

void buildSpecification()
{
    getConfiguration("Debug").assign(TreatModuleAsSource::NO, ConfigType::DEBUG);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION