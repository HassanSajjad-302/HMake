#include "Configure.hpp"

int main(int argc, char **argv)
{
    if (false)
    {
        /*        setBoolsAndSetRunDir(argc, argv);
                Configuration release{"Release"};
                release.ASSIGN(CxxSTD::V_LATEST, ConfigType::RELEASE);

                Configuration debug{"Debug"};
                debug.ASSIGN(CxxSTD::V_LATEST, ConfigType::DEBUG);
                auto func = [](Configuration &configuration) {
                                CppSourceTarget &fmt = configuration.GetLibCpp("fmt")
                                                           .SOURCE_FILES("fmt/src/format.cc", "fmt/src/os.cc")
                                                           .PUBLIC_INCLUDES("fmt/include");

                                CppSourceTarget &hconfigure = configuration.GetLibCpp("hconfigure")
                                                                  .SOURCE_DIRECTORIES("hconfigure/src/", ".*")
                                                                  .PUBLIC_INCLUDES("hconfigure/header",
           "cxxopts/include", "json/include") .PUBLIC_LIBRARIES(fmt.linkOrArchiveTarget);

                                configuration.GetExeCpp("hhelper")
                                    .SOURCE_FILES("hhelper/src/main.cpp")
                                    .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_HEADER", addEscapedQuotes(srcDir +
                       "hconfigure/header/")) .PRIVATE_COMPILE_DEFINITION("JSON_HEADER", addEscapedQuotes(srcDir +
                       "json/include/")) .PRIVATE_COMPILE_DEFINITION("FMT_HEADER", addEscapedQuotes(srcDir +
           "fmt/include/")) .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_STATIC_LIB_DIRECTORY", addEscapedQuotes(configureDir
           + "0/hconfigure/")) .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_STATIC_LIB_PATH", addEscapedQuotes(configureDir +
           "0/hconfigure/hconfigure.lib")) .PRIVATE_COMPILE_DEFINITION("FMT_STATIC_LIB_DIRECTORY",
           addEscapedQuotes(configureDir + "0/fmt/")) .PRIVATE_COMPILE_DEFINITION("FMT_STATIC_LIB_PATH",
           addEscapedQuotes(configureDir + "0/fmt/fmt.lib")) .PUBLIC_LIBRARIES(hconfigure.linkOrArchiveTarget);

                                configuration.GetExeCpp("hbuild")
                                    .SOURCE_FILES("hbuild/src/main.cpp")
                                    .PRIVATE_LIBRARIES(hconfigure.linkOrArchiveTarget);

                    configuration.GetExeCpp("HMakeHelper")
                        .SOURCE_FILES("hmake.cpp")
                        .PRIVATE_LIBRARIES(hconfigure.linkOrArchiveTarget);
                };
                func(release);
                func(debug);
                configureOrBuild();*/
    }
    else
    {
        setBoolsAndSetRunDir(argc, argv);
        Configuration debug{"Debug"};
        Configuration release{"Release"};
        Configuration arm("arm");

        CxxSTD cxxStd = debug.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b;
        /*        debug.compilerFeatures.compiler.bTPath =
                    "C:\\Program Files\\Microsoft Visual
           Studio\\2022\\Community\\VC\\Tools\\Llvm\\x64\\bin\\clang-cl";*/
        /*        debug.linkerFeatures.linker.bTPath =
                    "C:\\Program Files\\Microsoft Visual
           Studio\\2022\\Community\\VC\\Tools\\Llvm\\x64\\bin\\lld-link.exe";*/
        /*        debug.linkerFeatures.standardLibraryDirectories.emplace(
                    Node::getNodeFromString("C:\\Program Files\\Microsoft Visual "
                                            "Studio\\2022\\Community\\VC\\Tools\\Llvm\\x64\\lib\\clang\\15.0.1\\lib\\windows\\",
                                            false));*/
        debug.ASSIGN(cxxStd, TreatModuleAsSource::NO, TranslateInclude::YES, ConfigType::DEBUG, AddressSanitizer::OFF,
                     RuntimeDebugging::OFF);
        release.ASSIGN(cxxStd, TranslateInclude::YES, TreatModuleAsSource::NO, ConfigType::RELEASE);
        arm.ASSIGN(cxxStd, Arch::ARM, TranslateInclude::YES, ConfigType::RELEASE, TreatModuleAsSource::NO);
        /*        debug.compilerFeatures.requirementCompilerFlags += "--target=x86_64-pc-windows-msvc ";
                debug.linkerFeatures.requirementLinkerFlags += "--target=x86_64-pc-windows-msvc";*/
        // configuration.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");

        auto fun = [](Configuration &configuration) {
            DSC<CppSourceTarget> &fmt = configuration.GetCppObjectDSC("fmt");
            fmt.getSourceTarget().MODULE_FILES("fmt/src/format.cc", "fmt/src/os.cc").PUBLIC_HU_INCLUDES("fmt/include");

            DSC<CppSourceTarget> &hconfigure = configuration.GetCppObjectDSC("hconfigure").PUBLIC_LIBRARIES(&fmt);
            hconfigure.getSourceTarget()
                .MODULE_DIRECTORIES("hconfigure/src/", ".*")
                .PUBLIC_HU_INCLUDES("hconfigure/header", "cxxopts/include", "json/include")
                .setModuleScope(fmt.getSourceTargetPointer());

            DSC<CppSourceTarget> &hhelper = configuration.GetCppExeDSC("hhelper").PUBLIC_LIBRARIES(&hconfigure);
            hhelper.getSourceTarget()
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
                .setModuleScope(fmt.getSourceTargetPointer());

            DSC<CppSourceTarget> &hbuild = configuration.GetCppExeDSC("hbuild").PUBLIC_LIBRARIES(&hconfigure);
            hbuild.getSourceTarget().MODULE_FILES("hbuild/src/main.cpp").setModuleScope(fmt.getSourceTargetPointer());
        };

        fun(debug);
        /*        fun(release);
                fun(arm);*/
        configureOrBuild();
    }
}
