#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    if (config.name == "modules")
    {
        config.stdCppTarget->getSourceTarget().interfaceFiles("std.cpp", "std");
        config.getCppExeDSC("app").getSourceTarget().moduleFiles("main.cpp");
    }
    else
    {
        CppTarget &t = config.getCppExeDSC("app2").getSourceTarget().moduleFiles("main2.cpp");
        config.stdCppTarget->getSourceTarget().publicBigHus.emplace_back(nullptr);
        config.stdCppTarget->getSourceTarget().makeHeaderFileAsUnit("windows.h", true, true);
    }
}

void buildSpecification()
{
    getConfiguration("modules").assign(IsCppMod::YES, StdAsHeaderUnit::NO);
    getConfiguration("hu").assign(IsCppMod::YES, BigHeaderUnit::YES);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION