#include "Configure.hpp"

void buildSpecification()
{
    Configuration &debug = GetConfiguration("Debug");

    CxxSTD cxxStd = debug.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_23;

    debug.ASSIGN(cxxStd, TreatModuleAsSource::NO, ConfigType::DEBUG);

    // configuration.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");

    auto configureFunc = [](Configuration &configuration) {
        DSC<CppSourceTarget> &lib4 = configuration.GetCppStaticDSC("lib4");
        lib4.getSourceTarget().setModuleScope().PUBLIC_HU_INCLUDES("lib4/public").PRIVATE_HU_INCLUDES("lib4/private");
        configuration.moduleScope = lib4.getSourceTargetPointer();

        bool useModule = CacheVariable<bool>("use-module", true).value;
        if (useModule)
        {
            lib4.getSourceTarget()
                .MODULE_DIRECTORIES_RG("lib4/private", ".*cpp")
                .PRIVATE_COMPILE_DEFINITION("USE_MODULE", "");
        }
        else
        {
            lib4.getSourceTarget().SOURCE_DIRECTORIES_RG("lib4/private", ".*cpp");
        }

        DSC<CppSourceTarget> &lib3 = configuration.GetCppStaticDSC("lib3").PUBLIC_LIBRARIES(&lib4);
        lib3.getSourceTarget().MODULE_DIRECTORIES_RG("lib3/private", ".*cpp").PUBLIC_HU_INCLUDES("lib3/public");

        DSC<CppSourceTarget> &lib2 = configuration.GetCppStaticDSC("lib2").PRIVATE_LIBRARIES(&lib3);
        lib2.getSourceTarget().MODULE_DIRECTORIES_RG("lib2/private", ".*cpp").PUBLIC_INCLUDES("lib2/public");

        DSC<CppSourceTarget> &lib1 = configuration.GetCppStaticDSC("lib1").PUBLIC_LIBRARIES(&lib2);
        lib1.getSourceTarget().SOURCE_DIRECTORIES_RG("lib1/private", ".*cpp").PUBLIC_INCLUDES("lib1/public");

        DSC<CppSourceTarget> &app = configuration.GetCppExeDSC("app").PRIVATE_LIBRARIES(&lib1);
        app.getSourceTarget().SOURCE_FILES("main.cpp");
    };

    configureFunc(debug);
}

MAIN_FUNCTION