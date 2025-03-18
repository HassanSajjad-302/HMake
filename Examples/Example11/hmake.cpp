#include "Configure.hpp"

void buildSpecification()
{
    getCppExeDSC("app")
        .getSourceTarget()
        .publicHUIncludes("ball_pit/3rd_party/olcPixelGameEngine")
        .rModuleDirs("ball_pit/modules", "ball_pit/src")
        .assign(CxxSTD::V_LATEST);
}

MAIN_FUNCTION