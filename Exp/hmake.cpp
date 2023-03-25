#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);
    unsigned short count = 1000;
    vector<Configuration> confs;
    confs.reserve(count);

    for (unsigned short i = 0; i < count; ++i)
    {
        Configuration &conf = confs.emplace_back(std::to_string(i));
        if(conf.selectiveBuild)
        {
            DSC<CppSourceTarget> &lib = conf.GetCppStaticDSC("lib");
            lib.getSourceTarget().SOURCE_FILES("lib/lib.cpp").PUBLIC_INCLUDES("lib/");
            DSC<CppSourceTarget> &app = conf.GetCppExeDSC("app").PRIVATE_LIBRARIES(&lib);
            app.getSourceTarget().SOURCE_FILES("main.cpp");
        }
    }
    configureOrBuild();
}