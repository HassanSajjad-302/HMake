#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);
    GetExeCpp("app").SOURCE_FILES("main.cpp").ASSIGN(RTTI::OFF);
    configureOrBuild();
}
