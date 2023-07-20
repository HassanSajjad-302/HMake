#include "Configure.hpp"

void buildSpecification()
{
    GetCppExeDSC("app").getSourceTarget().SOURCE_FILES("main.cpp").ASSIGN(RTTI::OFF);
}

MAIN_FUNCTION