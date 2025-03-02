#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    config.getCppExeDSC("app").getSourceTarget().moduleFiles("main.cpp", "std.cpp");
    config.getCppExeDSC("app2")
        .getSourceTarget()
        .moduleFiles("main2.cpp")
        .initializeUseReqInclsFromReqIncls()
        .initializeHuDirsFromReqIncls();
}

void buildSpecification()
{
    getConfiguration();
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION