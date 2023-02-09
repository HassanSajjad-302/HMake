#include "Configure.hpp"

int main(int argc, char **argv)
{

    bool s = true;
    if (true)
    {
        setBoolsAndSetRunDir(argc, argv);
        Variant variant{"Release"};
        variant.privateCompilerFlags += " -std=c++2b";
        // variant.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");

        CppSourceTarget &fmt =
            GetLibCpp("fmt").SOURCE_FILES("fmt/src/format.cc", "fmt/src/os.cc").PUBLIC_INCLUDES("fmt/include");

        CppSourceTarget &hconfigure = GetLibCpp("hconfigure")
                                          .SOURCE_DIRECTORIES("hconfigure/src/", ".*")
                                          .PUBLIC_INCLUDES("hconfigure/header", "cxxopts/include", "json/include")
                                          .PUBLIC_LIBRARIES(fmt.linkOrArchiveTarget);

        GetExeCpp("hhelper")
            .SOURCE_FILES("hhelper/src/main.cpp")
            .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_HEADER", addEscapedQuotes(srcDir + "hconfigure/header/"))
            .PRIVATE_COMPILE_DEFINITION("JSON_HEADER", addEscapedQuotes(srcDir + "json/include/"))
            .PRIVATE_COMPILE_DEFINITION("FMT_HEADER", addEscapedQuotes(srcDir + "fmt/include/"))
            .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_STATIC_LIB_DIRECTORY",
                                        addEscapedQuotes(configureDir + "0/hconfigure/"))
            .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_STATIC_LIB_PATH",
                                        addEscapedQuotes(configureDir + "0/hconfigure/hconfigure.lib"))
            .PRIVATE_COMPILE_DEFINITION("FMT_STATIC_LIB_DIRECTORY", addEscapedQuotes(configureDir + "0/fmt/"))
            .PRIVATE_COMPILE_DEFINITION("FMT_STATIC_LIB_PATH", addEscapedQuotes(configureDir + "0/fmt/fmt.lib"))
            .PUBLIC_LIBRARIES(hconfigure.linkOrArchiveTarget);

        GetExeCpp("hbuild").SOURCE_FILES("hbuild/src/main.cpp").PRIVATE_LIBRARIES(hconfigure.linkOrArchiveTarget);

        configureOrBuild();
    }
    else
    {
        setBoolsAndSetRunDir(argc, argv);
        Variant variant{"Release"};
        variant.privateCompilerFlags += " -std=c++2b";
        // variant.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");

        CppSourceTarget &fmt =
            GetLibCpp("fmt").MODULE_FILES("fmt/src/format.cc", "fmt/src/os.cc").PUBLIC_HU_INCLUDES("fmt/include");

        CppSourceTarget &hconfigure = GetLibCpp("hconfigure")
                                          .MODULE_DIRECTORIES("hconfigure/src/", ".*")
                                          .PUBLIC_HU_INCLUDES("hconfigure/header", "cxxopts/include", "json/include")
                                          .PUBLIC_LIBRARIES(fmt.linkOrArchiveTarget);

        GetExeCpp("hhelper")
            .MODULE_FILES("hhelper/src/main.cpp")
            .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_HEADER", addEscapedQuotes(srcDir + "hconfigure/header/"))
            .PRIVATE_COMPILE_DEFINITION("JSON_HEADER", addEscapedQuotes(srcDir + "json/include/"))
            .PRIVATE_COMPILE_DEFINITION("FMT_HEADER", addEscapedQuotes(srcDir + "fmt/include/"))
            .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_STATIC_LIB_DIRECTORY",
                                        addEscapedQuotes(configureDir + "0/hconfigure/"))
            .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_STATIC_LIB_PATH",
                                        addEscapedQuotes(configureDir + "0/hconfigure/hconfigure.lib"))
            .PRIVATE_COMPILE_DEFINITION("FMT_STATIC_LIB_DIRECTORY", addEscapedQuotes(configureDir + "0/fmt/"))
            .PRIVATE_COMPILE_DEFINITION("FMT_STATIC_LIB_PATH", addEscapedQuotes(configureDir + "0/fmt/fmt.lib"))
            .PUBLIC_LIBRARIES(hconfigure.linkOrArchiveTarget);

        GetExeCpp("hbuild").MODULE_FILES("hbuild/src/main.cpp").PRIVATE_LIBRARIES(hconfigure.linkOrArchiveTarget);

        configureOrBuild();
    }
}
