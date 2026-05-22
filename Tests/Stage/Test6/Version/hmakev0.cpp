#include "Configure.hpp"
#include "CustomCodeGenerator.hpp"

void configurationSpecification(Configuration &config)
{
    // this target has a hu which includes a header-file
    DSC<CppTarget> &toolDep = config.getCppStaticDSC("tooldep");
    toolDep.getSourceTarget()
        .publicHeaderUnits("tool-hu.hpp", "tool-hu.hpp")
        .publicHeaderFiles("tool-hu-header.hpp", "tool-hu-header.hpp");

    // The tool depends on a tool-dep. has a header-unit include from that tool-dep and has 2 module-files
    DSC<CppTarget> &tool = config.getCppExeDSC("tool").privateDeps(toolDep);
    tool.getSourceTarget().moduleFiles("tool.cpp", "tool2.cpp");

    HeaderGen *headerGen = new HeaderGen(config.name + "/IncGen", &tool.getLOAT(), "CAT", "value.txt");

    DSC<CppTarget> &app = config.getCppExeDSC("app");
    CppMod &appHu = app.getSourceTarget()
                         .moduleFiles("app.cpp", "app2.cpp")
                         .privateHeaderFiles("output.h", headerGen->outputHeader)
                         .privateHeaderUnits("app-hu.hpp", "app-hu.hpp")
                         .getCppHeaderUnit("app-hu.hpp", true, false);

    appHu.realBTargets[0].addDep<BTargetType::UNKNOWN>(&headerGen->realBTargets[0]);
}

void buildSpecification()
{
    getConfiguration().assign(ConfigType::DEBUG, IsCppMod::YES);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION