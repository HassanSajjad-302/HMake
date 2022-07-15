#include "Configure.hpp"

int main()
{
    Cache::initializeCache();
    Project project;

    ProjectVariant variant{};
    Executable app("app", variant);
    // Following adds module source to the target. If a target has module source it is a module target. Otherwise, it is
    // a source target. A module target cannot be a dependency.

    // Simplifying a bit but even a project of 10k module files can be compiled with a very short hmake.cpp file because
    // dependencies are automatically sorted between the module files. e.g. the following line to add all the
    // source-code. ADD_MODULE_DIR_TO_TARGET(app, ".", "*.cpp");
    ADD_MODULE_DIR_TO_TARGET(app, "Mod_Src/", R"(^(?!hmake\.cpp$).*)");
    ADD_PUBLIC_CFD_TO_TARGET(app, "/std:c++20 /experimental:module");
    ADD_EXECUTABLES_TO_VARIANT(variant, app);
    project.projectVariants.push_back(variant);
    project.configure();
}
