#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    if (config.name == "modules")
    {
        config.getCppExeDSC("app").getSourceTarget().moduleFiles("main.cpp", "std.cpp");
    }
    else
    {
        config.getCppExeDSC("app2").getSourceTarget().moduleFiles("main2.cpp");
    }
}

void buildSpecification()
{
    getConfiguration("modules");
    getConfiguration("hu");
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION