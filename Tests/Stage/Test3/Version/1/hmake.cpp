#include "Configure.hpp"

void configurationSpecification(Configuration &debug)
{
    debug.assign(debug.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b);
    DSC<CppSourceTarget> &lib4 = debug.getCppStaticDSC("lib4");
    lib4.getSourceTarget()
        .sourceDirectoriesRE("lib4/private", ".*cpp")
        .publicIncludes("lib4/public")
        .privateIncludes("lib4/private");

    DSC<CppSourceTarget> &lib3 = debug.getCppStaticDSC("lib3").publicLibraries(&lib4);
    lib3.getSourceTarget().moduleDirectoriesRE("lib3/private", ".*cpp").publicHUIncludes("lib3/public");

    DSC<CppSourceTarget> &lib2 = debug.getCppStaticDSC("lib2").privateLibraries(&lib3);
    lib2.getSourceTarget().sourceDirectoriesRE("lib2/private", ".*cpp").publicIncludes("lib2/public");

    DSC<CppSourceTarget> &lib1 = debug.getCppStaticDSC("lib1").publicLibraries(&lib2);
    lib1.getSourceTarget().sourceDirectoriesRE("lib1/private", ".*cpp").publicIncludes("lib1/public");

    debug.getCppExeDSC("app").privateLibraries(&lib1).getSourceTarget().sourceFiles("main.cpp");
}

void buildSpecification()
{
    getConfiguration("Debug").assign(TreatModuleAsSource::NO, ConfigType::DEBUG);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION