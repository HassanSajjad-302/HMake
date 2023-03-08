#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);
    Configuration debug{"Debug"};

    CxxSTD cxxStd = debug.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_23;

    debug.ASSIGN(cxxStd, TreatModuleAsSource::YES, ConfigType::DEBUG);

    // configuration.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");

    auto configureFunc = [](Configuration &configuration) {
        DSC<CppSourceTarget> &app = configuration.GetCppExeDSC("app");
        app.getSourceTarget().SOURCE_FILES("main.cpp");
    };

    configureFunc(debug);
    configureOrBuild();
}