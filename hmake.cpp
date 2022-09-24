#include "Configure.hpp"
#include "iostream"

int main()
{
    Cache::initializeCache();
    flags.compilerFlags[BTFamily::MSVC] = " /std:c++latest";
    flags.compilerFlags[BTFamily::GCC] = " -std=c++2b";

    Project project;
    ProjectVariant variant{project};

    Library fmt("fmt", variant);
    fmt.SOURCE_FILES("fmt/src/format.cc", "fmt/src/os.cc").PUBLIC_INCLUDES("fmt/include");

    Library hconfigure("hconfigure", variant);
    hconfigure.SOURCE_FILES("hconfigure/src/Configure.cpp")
        .PUBLIC_INCLUDES("hconfigure/header", "cxxopts/include", "json/include")
        .PUBLIC_LIBRARIES(&fmt);

    Executable hhelper("hhelper", variant);
    hhelper.SOURCE_FILES("hhelper/src/main.cpp")
        .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_HEADER", addEscapedQuotes(srcDir + "hconfigure/header/"))
        .PRIVATE_COMPILE_DEFINITION("JSON_HEADER", addEscapedQuotes(srcDir + "json/include/"))
        .PRIVATE_COMPILE_DEFINITION("FMT_HEADER", addEscapedQuotes(srcDir + "fmt/include/"))
        .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_STATIC_LIB_DIRECTORY", addEscapedQuotes(configureDir + "0/hconfigure/"))
        .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_STATIC_LIB_PATH",
                                    addEscapedQuotes(configureDir + "0/hconfigure/hconfigure.lib"))
        .PRIVATE_LIBRARIES(&hconfigure);

    Executable hbuild("hbuild", variant);
    hbuild.SOURCE_FILES("hbuild/src/BBuild.cpp", "hbuild/src/main.cpp")
        .PRIVATE_INCLUDES("hbuild/header/")
        .PRIVATE_LIBRARIES(&hconfigure);

    project.configure();
}
