#include "Configure.hpp"
#include "iostream"

int main()
{
    Cache::initializeCache();
    flags.compilerFlags[BTFamily::MSVC] = " /std:c++latest";
    flags.compilerFlags[BTFamily::GCC] = " -std=c++2b";

    Project project;
    ProjectVariant variant{};

    Library fmt("fmt", variant);
    Library hconfigure("hconfigure", variant);
    Executable hhelper("hhelper", variant);
    Executable hbuild("hbuild", variant);

    ADD_SRC_FILES_TO_TARGET(fmt, "fmt/src/format.cc", "fmt/src/os.cc");
    ADD_PUBLIC_IDDS_TO_TARGET(fmt, "fmt/include");

    ADD_SRC_FILES_TO_TARGET(hconfigure, "hconfigure/src/Configure.cpp");
    ADD_PUBLIC_IDDS_TO_TARGET(hconfigure, "hconfigure/header", "cxxopts/include", "json/include");
    ADD_PUBLIC_LIB_DEPS_TO_TARGET(hconfigure, fmt);

    ADD_SRC_FILES_TO_TARGET(hhelper, "hhelper/src/main.cpp");
    ADD_PRIVATE_CDD_TO_TARGET(hhelper, "HCONFIGURE_HEADER", addEscapedQuotes(srcDir + "hconfigure/header/"));
    ADD_PRIVATE_CDD_TO_TARGET(hhelper, "JSON_HEADER", addEscapedQuotes(srcDir + "json/include/"));
    ADD_PRIVATE_CDD_TO_TARGET(hhelper, "FMT_HEADER", addEscapedQuotes(srcDir + "fmt/include/"));
    ADD_PRIVATE_CDD_TO_TARGET(hhelper, "HCONFIGURE_STATIC_LIB_DIRECTORY",
                              addEscapedQuotes(configureDir + "0/hconfigure/"));
    ADD_PRIVATE_CDD_TO_TARGET(hhelper, "HCONFIGURE_STATIC_LIB_PATH",
                              addEscapedQuotes(configureDir + "0/hconfigure/hconfigure.lib"));
    ADD_PRIVATE_LIB_DEPS_TO_TARGET(hhelper, hconfigure);

    ADD_SRC_FILES_TO_TARGET(hbuild, "hbuild/src/BBuild.cpp", "hbuild/src/main.cpp");
    ADD_PRIVATE_IDDS_TO_TARGET(hbuild, "hbuild/header/");
    ADD_PRIVATE_LIB_DEPS_TO_TARGET(hbuild, hconfigure);

    ADD_LIBRARIES_TO_VARIANT(variant, hconfigure);
    ADD_EXECUTABLES_TO_VARIANT(variant, hhelper, hbuild);
    project.projectVariants.emplace_back(variant);

    project.configure();
}
