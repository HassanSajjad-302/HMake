#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    config.getCppExeDSC("app").getSourceTarget().moduleDirs("Mod_Src/");
}

void buildSpecification()
{
    getConfiguration().assign(TreatModuleAsSource::NO);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION