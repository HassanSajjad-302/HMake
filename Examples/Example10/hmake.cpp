#include "Configure.hpp"

int main()
{
    Cache::initializeCache();
    Project project;

    ProjectVariant variant{};

    Library olcPixelGameEngine("olcPixelGameEngine", variant);
    ADD_SRC_FILES_TO_TARGET(olcPixelGameEngine, "src/3rd_party/olcPixelGameEngine.cpp");
    ADD_PUBLIC_IDDS_TO_TARGET(olcPixelGameEngine, "3rd_party/olcPixelGameEngine");

    Executable app("app", variant);
    ADD_MODULE_DIR_TO_TARGET(app, "./modules/", ".*");
    ADD_MODULE_DIR_TO_TARGET(app, "./src", ".*");
    ADD_PUBLIC_CFD_TO_TARGET(app, "/std:c++20 /experimental:module ");
    ADD_PRIVATE_LIB_DEPS_TO_TARGET(app, olcPixelGameEngine);

    ADD_EXECUTABLES_TO_VARIANT(variant, app);

    project.projectVariants.push_back(variant);
    project.configure();
}
