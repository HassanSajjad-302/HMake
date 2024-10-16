#include "Configure.hpp"

void buildSpecification()
{
    Configuration &debug = getConfiguration("Debug");

    CxxSTD cxxStd = debug.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b;

    debug.assign(cxxStd, TreatModuleAsSource::YES, ConfigType::DEBUG);

    // configuration.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");

    auto configureFunc = [](Configuration &configuration) {
        DSC<CppSourceTarget> &lib4 = configuration.getCppStaticDSC("lib4");
        lib4.getSourceTarget().sourceDirectoriesRE("lib4/private", ".*cpp").publicIncludes("lib4/public");

        DSC<CppSourceTarget> &lib3 = configuration.getCppStaticDSC("lib3").publicLibraries(&lib4);
        lib3.getSourceTarget().sourceDirectoriesRE("lib3/private", ".*cpp").publicIncludes("lib3/public");

        DSC<CppSourceTarget> &lib2 = configuration.getCppStaticDSC("lib2").privateLibraries(&lib3);
        lib2.getSourceTarget().sourceDirectoriesRE("lib2/private", ".*cpp").publicIncludes("lib2/public");

        DSC<CppSourceTarget> &lib1 = configuration.getCppStaticDSC("lib1").publicLibraries(&lib2);
        lib1.getSourceTarget().sourceDirectoriesRE("lib1/private", ".*cpp").publicIncludes("lib1/public");

        DSC<CppSourceTarget> &app = configuration.getCppExeDSC("app").privateLibraries(&lib1);
        app.getSourceTarget().sourceFiles("main.cpp");
    };

    configureFunc(debug);
}

MAIN_FUNCTION