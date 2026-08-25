#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    config.assign(config.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b);

    DSC<CppTarget> &lib4 = config.getCppStaticDSC("lib4");
    lib4.getSourceTarget().sourceDirsRE("lib4/private", ".*cpp").publicIncludes("lib4/public");

    DSC<CppTarget> &lib3 = config.getCppStaticDSC("lib3").publicDeps(lib4);
    lib3.getSourceTarget().sourceDirsRE("lib3/private", ".*cpp").publicIncludes("lib3/public");

    DSC<CppTarget> &lib2 = config.getCppStaticDSC("lib2").privateDeps(lib3);
    lib2.getSourceTarget().sourceDirsRE("lib2/private", ".*cpp").publicIncludes("lib2/public");

    DSC<CppTarget> &lib1 = config.getCppStaticDSC("lib1").publicDeps(lib2);
    lib1.getSourceTarget().sourceDirsRE("lib1/private", ".*cpp").publicIncludes("lib1/public");

    config.getCppExeDSC("app").privateDeps(lib1).getSourceTarget().sourceFiles("main.cpp");
}

void buildSpecification()
{
    getConfiguration("Release").assign(IsCppMod::NO, ConfigType::RELEASE, CxxSTD::V_98);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION