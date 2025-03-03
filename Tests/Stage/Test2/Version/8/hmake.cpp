#include "Configure.hpp"

void configurationSpecification(Configuration &debug)
{
    debug.assign(debug.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b);

    DSC<CppSourceTarget> &lib4 = debug.getCppStaticDSC("lib4");
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

    DSC<CppSourceTarget> &lib3 = debug.getCppStaticDSC("lib3").publicLibraries(&lib4);
    lib3.getSourceTarget().sourceDirectoriesRE("lib3/private", ".*cpp").publicIncludes("lib3/public");

    DSC<CppSourceTarget> &lib2 = debug.getCppStaticDSC("lib2").privateLibraries(&lib3);
    lib2.getSourceTarget().sourceDirectoriesRE("lib2/private", ".*cpp").publicIncludes("lib2/public");

    DSC<CppSourceTarget> &lib1 = debug.getCppStaticDSC("lib1").publicLibraries(&lib2);
    lib1.getSourceTarget().sourceDirectoriesRE("lib1/private", ".*cpp").publicIncludes("lib1/public");

    DSC<CppSourceTarget> &app = debug.getCppExeDSC("app").privateLibraries(&lib1);
    app.getSourceTarget().sourceFiles("main.cpp");
}

void buildSpecification()
{
    getConfiguration("Debug").assign(TreatModuleAsSource::YES, ConfigType::DEBUG);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION