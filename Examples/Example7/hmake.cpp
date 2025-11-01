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
        CppSourceTarget &t = config.getCppExeDSC("app2").getSourceTarget().moduleFiles("main2.cpp");
        config.stdCppTarget->getSourceTarget().publicBigHu.emplace_back(nullptr);
        config.stdCppTarget->getSourceTarget().makeHeaderFileAsUnit("windows.h", true, true);
    }
}

void buildSpecification()
{
    getConfiguration("modules").assign(TreatModuleAsSource::NO, StdAsHeaderUnit::NO);
    getConfiguration("hu").assign(TreatModuleAsSource::NO, BigHeaderUnit::YES);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION