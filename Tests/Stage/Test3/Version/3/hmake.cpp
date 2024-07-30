#include "Configure.hpp"

void buildSpecification()
{
    Configuration &debug = getConfiguration("Debug");

    CxxSTD cxxStd = debug.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_23;

    debug.assign(cxxStd, TreatModuleAsSource::NO, ConfigType::DEBUG);

    // configuration.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");

    auto configureFunc = [](Configuration &configuration) {
        DSC<CppSourceTarget> &lib4 = configuration.getCppStaticDSC("lib4");
        lib4.getSourceTarget()
            .moduleDirectoriesRE("lib4/private", ".*cpp")
            .publicHUIncludes("lib4/public")
            .privateHUIncludes("lib4/private");

        DSC<CppSourceTarget> &lib3 = configuration.getCppStaticDSC("lib3").publicLibraries(&lib4);
        lib3.getSourceTarget().moduleDirectoriesRE("lib3/private", ".*cpp").publicHUIncludes("lib3/public");

        DSC<CppSourceTarget> &lib2 = configuration.getCppStaticDSC("lib2").privateLibraries(&lib3);
        lib2.getSourceTarget().moduleDirectoriesRE("lib2/private", ".*cpp").publicIncludes("lib2/public");

        DSC<CppSourceTarget> &lib1 = configuration.getCppStaticDSC("lib1").publicLibraries(&lib2);
        lib1.getSourceTarget().sourceDirectoriesRE("lib1/private", ".*cpp").publicIncludes("lib1/public");

        DSC<CppSourceTarget> &app = configuration.getCppExeDSC("app").privateLibraries(&lib1);
        app.getSourceTarget().sourceFiles("main.cpp");
    };

    configureFunc(debug);
}

MAIN_FUNCTION