#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);
    unsigned short count = 1000;
    vector<Configuration> confs;
    confs.reserve(count);

    for (unsigned short i = 0; i < count; ++i)
    {
        confs.emplace_back(std::to_string(i));
        DSC<CppSourceTarget> &lib = confs[i].GetCppStaticDSC("lib");
        lib.getSourceTarget().SOURCE_FILES("lib/lib.cpp").PUBLIC_INCLUDES("lib/");
        DSC<CppSourceTarget> &app = confs[i].GetCppExeDSC("app").PRIVATE_LIBRARIES(&lib);
        app.getSourceTarget().SOURCE_FILES("main.cpp");
    }
    configureOrBuild();
}