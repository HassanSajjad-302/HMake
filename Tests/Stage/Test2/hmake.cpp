#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    config.assign(config.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b);

    DSC<CppSourceTarget> &lib4 = config.getCppStaticDSC("lib4");
    lib4.getSourceTarget().sourceDirectoriesRE("lib4/private", ".*cpp").publicIncludes("lib4/public");

    DSC<CppSourceTarget> &lib3 = config.getCppStaticDSC("lib3").publicLibraries(&lib4);
    lib3.getSourceTarget().sourceDirectoriesRE("lib3/private", ".*cpp").publicIncludes("lib3/public");

    DSC<CppSourceTarget> &lib2 = config.getCppStaticDSC("lib2").privateLibraries(&lib3);
    lib2.getSourceTarget().sourceDirectoriesRE("lib2/private", ".*cpp").publicIncludes("lib2/public");

    DSC<CppSourceTarget> &lib1 = config.getCppStaticDSC("lib1").publicLibraries(&lib2);
    lib1.getSourceTarget().sourceDirectoriesRE("lib1/private", ".*cpp").publicIncludes("lib1/public");

    config.getCppExeDSC("app").privateLibraries(&lib1).getSourceTarget().sourceFiles("main.cpp");
}

void buildSpecification()
{
    getConfiguration("Debug").assign(TreatModuleAsSource::YES, ConfigType::DEBUG);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION