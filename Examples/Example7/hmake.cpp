#include "Configure.hpp"

void buildSpecification()
{
    GetCppExeDSC("app").getSourceTarget().MODULE_FILES("main.cpp", "std.cpp");
    GetCppExeDSC("app2").getSourceTarget().MODULE_FILES("main2.cpp").assignStandardIncludesToPublicHUDirectories();
}

MAIN_FUNCTION