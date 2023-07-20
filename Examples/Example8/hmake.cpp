#include "Configure.hpp"

void buildSpecification()
{
    GetCppExeDSC("app").getSourceTarget().MODULE_DIRECTORIES("Mod_Src/").setModuleScope();
}

MAIN_FUNCTION