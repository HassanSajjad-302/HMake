#include "Configure.hpp"

void configurationSpecification(Configuration &config)
{
    DSC<CppSourceTarget> &libB = config.getCppStaticDSC("libB");
    libB.getSourceTarget().moduleFiles("B.cpp").publicHeaderUnits("B.hpp", "B.hpp");

    config.getCppExeDSC("appA").privateDeps(libB).getSourceTarget().moduleFiles("A.cpp").privateHeaderUnits("A.hpp",
                                                                                                            "A.hpp");
}

void buildSpecification()
{
    getConfiguration().assign(TreatModuleAsSource::NO);
    CALL_CONFIGURATION_SPECIFICATION
}

MAIN_FUNCTION