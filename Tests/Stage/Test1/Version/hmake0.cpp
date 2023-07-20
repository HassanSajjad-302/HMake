#include "Configure.hpp"

void buildSpecification()
{
    GetCppExeDSC("app").getSourceTarget().SOURCE_FILES("main.cpp");
}

MAIN_FUNCTION
