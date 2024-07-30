#include "Configure.hpp"

void buildSpecification()
{
    getCppExeDSC("app").getSourceTarget().moduleDirectories("Mod_Src/");
}

MAIN_FUNCTION