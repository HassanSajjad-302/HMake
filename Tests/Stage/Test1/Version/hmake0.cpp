#include "Configure.hpp"

void buildSpecification()
{
    getCppExeDSC("app").getSourceTarget().sourceFiles("main.cpp");
}

MAIN_FUNCTION
