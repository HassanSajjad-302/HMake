#include "Configure.hpp"

void buildSpecification()
{
    GetCppExeDSC("app")
        .getSourceTarget()
        .setModuleScope()
        .PUBLIC_HU_INCLUDES("3rd_party/olcPixelGameEngine")
        .R_MODULE_DIRECTORIES("modules/", "src/")
        .ASSIGN(CxxSTD::V_LATEST);
}

MAIN_FUNCTION