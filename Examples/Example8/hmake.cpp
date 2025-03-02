#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    config.getCppExeDSC("app").getSourceTarget().moduleDirectories("Mod_Src/");
}

void buildSpecification()
{
    getConfiguration();
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION