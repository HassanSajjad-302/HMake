#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    DSC<CppTarget> &libB = config.getCppStaticDSC("libB");
    libB.getSourceTarget().moduleFiles("B.cpp").publicHeaderUnits("B.hpp", "B.hpp");

    config.getCppExeDSC("appA").privateDeps(libB).getSourceTarget().moduleFiles("A.cpp").privateHeaderUnits("A.hpp",
                                                                                                            "A.hpp");
}

void buildSpecification()
{
    getConfiguration().assign(IsCppMod::YES);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION