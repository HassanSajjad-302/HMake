#include "Configure.hpp"

void buildSpecification()
{
    getCppExeDSC("app").getSourceTarget().sourceFiles("main.cpp").assign(RTTI::OFF);
}

MAIN_FUNCTION