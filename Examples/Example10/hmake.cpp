#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
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