#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    if (config.name == "modules")
    {
        config.getCppExeDSC("app").getSourceTarget().interfaceFiles("std.cpp", "std").moduleFiles("main.cpp");
    }
    else
    {
        config.getCppExeDSC("app2").getSourceTarget().moduleFiles("main2.cpp");
    }
}

void buildSpecification()
{
    getConfiguration("modules").assign(TreatModuleAsSource::NO, StdAsHeaderUnit::NO);
    getConfiguration("hu").assign(TreatModuleAsSource::NO);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION