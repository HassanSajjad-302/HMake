#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);
    vector<Configuration> confs;
    for (unsigned short i = 0; i < 5; ++i)
    {
        confs.emplace_back(std::to_string(i));
        confs[i].ASSIGN(TreatModuleAsSource::NO);
        auto fun = [](Configuration &configuration) {
            DSC<CppSourceTarget> &lib = configuration.GetCppStaticDSC("lib");
            lib.getSourceTarget().MODULE_FILES("lib/lib.cpp").PUBLIC_HU_INCLUDES("lib/");

            DSC<CppSourceTarget> &app = configuration.GetCppExeDSC("app").PUBLIC_LIBRARIES(&lib);
            app.getSourceTarget().MODULE_FILES("main.cpp").setModuleScope(lib.getSourceTargetPointer());
        };

        fun(confs[i]);
    }
    configureOrBuild();
}
