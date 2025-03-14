#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    config.assign(config.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b);

    DSC<CppSourceTarget> &lib4 = config.getCppStaticDSC("lib4");
    lib4.getSourceTarget().publicIncludes("lib4/public");
    bool useLib4Cpp = CacheVariable<bool>("use-lib4.cpp", true).value;
    if (useLib4Cpp)
    {
        lib4.getSourceTarget().sourceFiles("lib4/private/lib4.cpp");
    }
    else
    {
        lib4.getSourceTarget().sourceFiles("lib4/private/temp.cpp");
    }

    DSC<CppSourceTarget> &lib3 = config.getCppStaticDSC("lib3").publicDeps(lib4);
    lib3.getSourceTarget().sourceDirsRE("lib3/private", ".*cpp").publicIncludes("lib3/public");

    DSC<CppSourceTarget> &lib2 = config.getCppStaticDSC("lib2").privateDeps(lib3);
    lib2.getSourceTarget().sourceDirsRE("lib2/private", ".*cpp").publicIncludes("lib2/public");

    DSC<CppSourceTarget> &lib1 = config.getCppStaticDSC("lib1").publicDeps(lib2);
    lib1.getSourceTarget().sourceDirsRE("lib1/private", ".*cpp").publicIncludes("lib1/public");

    config.getCppExeDSC("app").privateDeps(lib1).getSourceTarget().sourceFiles("main.cpp");
}

void buildSpecification()
{
    getConfiguration("Debug").assign(TreatModuleAsSource::YES, ConfigType::DEBUG);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION