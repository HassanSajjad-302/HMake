#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);

    GetCppExeDSC("app")
        .getSourceTarget()
        .PUBLIC_INCLUDES("3rd_party/olcPixelGameEngine")
        .MODULE_DIRECTORIES("./modules/", ".*")
        .MODULE_DIRECTORIES("./src", ".*")
        .ASSIGN(CxxSTD::V_LATEST);

    configureOrBuild();
}
