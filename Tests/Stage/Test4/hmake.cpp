#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    config.getCppStaticDSC("aevain");
    config.getCppStaticDSC("lib2").getSourceTarget().moduleFiles("lib2/lib2.cpp");
    return;
    config.getCppStaticDSC("lib1").getSourceTarget().moduleFiles("lib1/lib1.cpp");
}

void buildSpecification()
{
    getConfiguration("hu");
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
