#include "Configure.hpp"

void buildSpecification()
{
    getCppExeDSC("app").getSourceTarget().moduleFiles("main.cpp", "std.cpp");
    getCppExeDSC("app2").getSourceTarget().moduleFiles("main2.cpp").initializeUseReqInclsFromReqIncls().initializeHuDirsFromReqIncls();
}

MAIN_FUNCTION