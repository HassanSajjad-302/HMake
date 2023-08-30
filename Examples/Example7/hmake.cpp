#include "Configure.hpp"

void buildSpecification()
{
    GetCppExeDSC("app").getSourceTarget().MODULE_FILES("main.cpp", "std.cpp").setModuleScope();
    GetCppExeDSC("app2")
        .getSourceTarget()
        .MODULE_FILES("main2.cpp")
        .setModuleScope()
        .assignStandardIncludesToPublicHUDirectories();
}

MAIN_FUNCTION