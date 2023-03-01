#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);
    Configuration pass{"Pass"};
    Configuration fail{"Fail"};
    Configuration headerUnitPass{"HeaderUnitPass"};
    Configuration headerUnitFail{"HeaderUnitFail"};
    CxxSTD cxxStd = pass.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_23;

    pass.ASSIGN(cxxStd, TreatModuleAsSource::YES, ConfigType::DEBUG);
    fail.ASSIGN(cxxStd, TreatModuleAsSource::YES, ConfigType::DEBUG, Define("FAIL_THE_BUILD"));
    headerUnitPass.ASSIGN(cxxStd, ConfigType::DEBUG, Define("USE_HEADER_UNIT"));
    headerUnitFail.ASSIGN(cxxStd, ConfigType::DEBUG, Define("FAIL_THE_BUILD"), Define("USE_HEADER_UNIT"));
    // configuration.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");

    auto configureFunc = [](Configuration &configuration) {
        CppSourceTarget &lib =
            configuration.GetLibCpp("lib")
                .MODULE_FILES("lib/func1.cpp" /*, "lib/func2.cpp", "lib/func3.cpp", "lib/func4.cpp"*/)
                .PUBLIC_HU_INCLUDES("lib/");
        /*configuration.GetLibCpp("lib").MODULE_DIRECTORIES("lib/", ".*cpp").PUBLIC_HU_INCLUDES("lib/");*/
        CppSourceTarget &app = configuration.GetExeCpp("app").MODULE_FILES("main.cpp").PRIVATE_LIBRARIES(&lib);
        configuration.setModuleScope(&app);
    };

    // configureFunc(pass);
    // configureFunc(fail);
    configureFunc(headerUnitPass);
    // configureFunc(headerUnitFail);
    configureOrBuild();
}