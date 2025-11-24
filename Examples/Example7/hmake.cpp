#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    if (config.name == "modules")
    {
        config.stdCppTarget->getSourceTarget().interfaceFiles("std.cpp", "std");
        // config.getCppExeDSC("app").getSourceTarget().moduleFiles("main.cpp");
    }
    else
    {
        config.getCppExeDSC("app2").getSourceTarget().moduleFiles("main2.cpp");
    }
}

void buildSpecification()
{
    // module build of std.ixx provided with the msvc lib crashing with this commit. can be produced on developer
    // powershell. https://pastebin.com/38Q3FmWh
    //  it was working before but is failing with this commit. cc0371f2a4f95614c35601f898dde7745120e8d1.
    // getConfiguration("modules").assign(IsCppMod::YES, StdAsHeaderUnit::NO, CxxSTD::V_20);
    getConfiguration("hu").assign(IsCppMod::YES, BigHeaderUnit::YES);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION