#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);
    Configuration debug{"Debug"};

    CxxSTD cxxStd = debug.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_23;

    debug.ASSIGN(cxxStd, TreatModuleAsSource::NO, ConfigType::RELEASE);

    // configuration.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");

    auto configureFunc = [](Configuration &configuration) {
        DSC<CppSourceTarget> &lib4 = configuration.GetCppStaticDSC("lib4");
        lib4.getSourceTarget().SOURCE_DIRECTORIES("lib4/private/", ".*cpp").PUBLIC_INCLUDES("lib4/public/");

        DSC<CppSourceTarget> &lib3 = configuration.GetCppStaticDSC("lib3").PUBLIC_LIBRARIES(&lib4);
        lib3.getSourceTarget().MODULE_DIRECTORIES("lib3/private/", ".*cpp").PUBLIC_HU_INCLUDES("lib3/public/");

        DSC<CppSourceTarget> &lib2 = configuration.GetCppStaticDSC("lib2").PRIVATE_LIBRARIES(&lib3);
        lib2.getSourceTarget().SOURCE_DIRECTORIES("lib2/private/", ".*cpp").PUBLIC_INCLUDES("lib2/public/");

        DSC<CppSourceTarget> &lib1 = configuration.GetCppStaticDSC("lib1").PUBLIC_LIBRARIES(&lib2);
        lib1.getSourceTarget().SOURCE_DIRECTORIES("lib1/private/", ".*cpp").PUBLIC_INCLUDES("lib1/public/");

        DSC<CppSourceTarget> &app = configuration.GetCppExeDSC("app").PRIVATE_LIBRARIES(&lib1);
        app.getSourceTarget().SOURCE_FILES("main.cpp");
    };

    configureFunc(debug);
    configureOrBuild();
}