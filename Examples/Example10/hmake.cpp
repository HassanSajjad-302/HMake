#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    // This tests the situation where two different targets have header-unit in the same dir. In that case
    // these should be specified in the configuration file

    DSC<CppSourceTarget> &libB = config.getCppStaticDSC("libB");
    libB.getSourceTarget().moduleFiles("B.cpp").headerUnits("B.hpp").publicIncludes(
        path(srcNode->filePath).string());

    config.getCppExeDSC("appA")
        .privateDeps(libB)
        .getSourceTarget()
        .moduleFiles("A.cpp")
        .headerUnits("A.hpp");
}

void buildSpecification()
{
    getConfiguration();
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION
