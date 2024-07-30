#include "Configure.hpp"

void buildSpecification()
{
    Configuration &debug = getConfiguration("Debug");

    CxxSTD cxxStd = debug.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b;

    debug.assign(cxxStd, TreatModuleAsSource::YES, ConfigType::DEBUG);

    // configuration.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");

    auto configureFunc = [](Configuration &configuration) {
        DSC<CppSourceTarget> &app = configuration.getCppExeDSC("app");
        app.getSourceTarget().sourceFiles("main.cpp");
    };

    configureFunc(debug);
}

MAIN_FUNCTION