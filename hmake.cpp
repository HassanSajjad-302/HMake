#include "Configure.hpp"

int main(int argc, char **argv)
{
    setBoolsAndSetRunDir(argc, argv);
    Variant variant{"Release"};
    variant.privateCompilerFlags += " /std:c++latest /translateInclude";
    // variant.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");
    Library fmt("fmt", variant);
    fmt.SOURCE_FILES("fmt/src/format.cc", "fmt/src/os.cc").PUBLIC_INCLUDES("fmt/include");

    Library hconfigure("hconfigure", variant);
    hconfigure.SOURCE_DIRECTORIES("hconfigure/src/", ".*")
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
        .PRIVATE_COMPILE_DEFINITION("FMT_STATIC_LIB_DIRECTORY", addEscapedQuotes(configureDir + "0/fmt/"))
        .PRIVATE_COMPILE_DEFINITION("FMT_STATIC_LIB_PATH", addEscapedQuotes(configureDir + "0/fmt/fmt.lib"))
        .PUBLIC_LIBRARIES(&hconfigure);

    Executable hbuild("hbuild", variant);
    hbuild.SOURCE_FILES("hbuild/src/main.cpp").PRIVATE_LIBRARIES(&hconfigure);

    configureOrBuild();
}
