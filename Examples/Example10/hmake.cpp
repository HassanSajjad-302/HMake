#include "Configure.hpp"

int main()
{
    Cache::initializeCache();
    Project project;

    ProjectVariant variant{project};

    Library olcPixelGameEngine("olcPixelGameEngine", variant);
    olcPixelGameEngine.SOURCE_FILES("src/3rd_party/olcPixelGameEngine.cpp")
        .PUBLIC_INCLUDES("3rd_party/olcPixelGameEngine");

    Executable app("app", variant);
    app.MODULE_DIRECTORIES("./modules/", ".*")
        .MODULE_DIRECTORIES("./src", ".*")
        .PUBLIC_COMPILER_FLAGS("/std:c++20 /experimental:module ")
        .PUBLIC_LIBRARIES(&olcPixelGameEngine);

    project.configure();
}
