#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);
    Configuration debug{"Debug"};
    Configuration release{"Release"};
    Configuration arm("arm");

    CxxSTD cxxStd = debug.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_23;

    debug.ASSIGN(cxxStd, TreatModuleAsSource::YES, ConfigType::DEBUG);
    release.ASSIGN(cxxStd, TranslateInclude::YES, TreatModuleAsSource::NO, ConfigType::RELEASE);
    arm.ASSIGN(cxxStd, Arch::ARM, TranslateInclude::YES, ConfigType::RELEASE, TreatModuleAsSource::NO);

    // configuration.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");

    auto configureFunc = [](Configuration &configuration) {
        DSC<CppSourceTarget> &lib4 = configuration.GetCppStaticDSC("lib4");
        lib4.getSourceTarget().MODULE_DIRECTORIES("lib4/private/", ".*cpp").PUBLIC_HU_INCLUDES("lib4/public/");

        DSC<CppSourceTarget> &lib3 = configuration.GetCppStaticDSC("lib3").PUBLIC_LIBRARIES(&lib4);
        lib3.getSourceTarget().MODULE_DIRECTORIES("lib3/private/", ".*cpp").PUBLIC_HU_INCLUDES("lib3/public/");

        DSC<CppSourceTarget> &lib2 = configuration.GetCppStaticDSC("lib2").PRIVATE_LIBRARIES(&lib3);
        lib2.getSourceTarget().MODULE_DIRECTORIES("lib2/private/", ".*cpp").PUBLIC_HU_INCLUDES("lib2/public/");

        DSC<CppSourceTarget> &lib1 = configuration.GetCppStaticDSC("lib1").PUBLIC_LIBRARIES(&lib2);
        lib1.getSourceTarget().MODULE_DIRECTORIES("lib1/private/", ".*cpp").PUBLIC_HU_INCLUDES("lib1/public/");

        DSC<CppSourceTarget> &app = configuration.GetCppExeDSC("app").PRIVATE_LIBRARIES(&lib1);
        app.getSourceTarget().MODULE_FILES("main.cpp");
    };

    configureFunc(debug);
    /*        configureFunc(release);
            configureFunc(arm);*/
    configureOrBuild();
}