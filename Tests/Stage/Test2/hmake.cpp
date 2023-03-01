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
        CppSourceTarget &lib4 = configuration.GetLibCpp("lib4")
                                    .MODULE_DIRECTORIES("lib4/private/", ".*cpp")
                                    .PUBLIC_HU_INCLUDES("lib4/public/");

        CppSourceTarget &lib3 = configuration.GetLibCpp("lib3")
                                    .MODULE_DIRECTORIES("lib3/private/", ".*cpp")
                                    .PUBLIC_HU_INCLUDES("lib3/public/")
                                    .PUBLIC_LIBRARIES(&lib4);
        CppSourceTarget &lib2 = configuration.GetLibCpp("lib2")
                                    .MODULE_DIRECTORIES("lib2/private/", ".*cpp")
                                    .PUBLIC_HU_INCLUDES("lib2/public/")
                                    .PRIVATE_LIBRARIES(&lib3);
        CppSourceTarget &lib1 = configuration.GetLibCpp("lib1")
                                    .MODULE_DIRECTORIES("lib1/private/", ".*cpp")
                                    .PUBLIC_HU_INCLUDES("lib1/public/")
                                    .PUBLIC_LIBRARIES(&lib2);

        configuration.GetExeCpp("app").MODULE_FILES("main.cpp").PRIVATE_LIBRARIES(&lib1);
    };

    configureFunc(debug);
    /*        configureFunc(release);
            configureFunc(arm);*/
    configureOrBuild();
}