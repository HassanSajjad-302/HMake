#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    config.getCppStaticDSC("aevain");
    config.getCppStaticDSC("lib2").getSourceTarget().moduleFiles("lib2/lib2.cpp");
    config.getCppStaticDSC("lib1").getSourceTarget().moduleFiles("lib1/lib1.cpp");
    return;
}

void buildSpecification()
{
    getConfiguration("hu");
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
