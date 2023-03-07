#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);
    GetCppExeDSC("app").getSourceTarget().SOURCE_FILES("main.cpp").ASSIGN(RTTI::OFF, );
    configureOrBuild();
}
