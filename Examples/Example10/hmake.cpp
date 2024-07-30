#include "Configure.hpp"

void buildSpecification()
{
    getCppExeDSC("app")
        .getSourceTarget()
        .publicHUIncludes("3rd_party/olcPixelGameEngine")
        .R_moduleDirectories("modules/", "src/")
        .assign(CxxSTD::V_LATEST);
}

MAIN_FUNCTION