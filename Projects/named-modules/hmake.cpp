#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    DSC<CppTarget> &manager = config.getCppStaticDSC("Manager");
    manager.getSourceTarget().moduleFiles("Manager.cpp").headerUnits("Manager.hpp", "BufferSize.hpp", "Messages.hpp");

    DSC<CppTarget> &buildSystem = config.getCppStaticDSC("BuildSystem").publicDeps(manager);
    buildSystem.getSourceTarget().moduleFiles("IPCManagerBS.cpp").headerUnits("IPCManagerBS.hpp");

    DSC<CppTarget> &compiler = config.getCppStaticDSC("Compiler").publicDeps(manager);
    compiler.getSourceTarget().moduleFiles("IPCManagerCompiler.cpp").headerUnits("IPCManagerCompiler.hpp");

    DSC<CppTarget> &testing = config.getCppStaticDSC("Testing").publicDeps(manager);
    testing.getSourceTarget().moduleFiles("Testing.cpp").headerUnits("Testing.hpp");

    // config.getCppExeDSC("BuildSystemTest").publicDeps(buildSystem,
    // testing).getSourceTarget().moduleFiles("BuildSystemTest.cpp");
    // config.getCppExeDSC("CompilerTest").publicDeps(compiler,
    // testing).getSourceTarget().moduleFiles("CompilerTest.cpp");
}

void buildSpecification()
{
    getConfiguration("modules").assign(TranslateInclude::YES);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION