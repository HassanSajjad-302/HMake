#include "Configure.hpp"

#include <functional>
#include <utility>

void configurationSpecification(Configuration &configuration)
{
    DSC<CppSourceTarget> &stdhu = configuration.GetCppObjectDSC("stdhu");
    stdhu.getSourceTarget().assignStandardIncludesToHUIncludes();

    DSC<CppSourceTarget> &fmt = configuration.GetCppObjectDSC("fmt");
    fmt.getSourceTarget().MODULE_FILES("fmt/src/format.cc", "fmt/src/os.cc").PUBLIC_HU_INCLUDES("fmt/include");

    DSC<CppSourceTarget> &hconfigure = configuration.GetCppObjectDSC("hconfigure").PUBLIC_LIBRARIES(&fmt);
    hconfigure.getSourceTarget()
        .MODULE_DIRECTORIES("hconfigure/src/", ".*")
        .PUBLIC_HU_INCLUDES("hconfigure/header", "cxxopts/include", "json/include");

    DSC<CppSourceTarget> &hhelper =
        configuration.GetCppExeDSC("hhelper").PUBLIC_LIBRARIES(&hconfigure).PRIVATE_LIBRARIES(&stdhu);
    hhelper.getSourceTarget()
        .MODULE_FILES("hhelper/src/main.cpp")
        .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_HEADER", addEscapedQuotes(srcDir + "hconfigure/header/"))
        .PRIVATE_COMPILE_DEFINITION("JSON_HEADER", addEscapedQuotes(srcDir + "json/include/"))
        .PRIVATE_COMPILE_DEFINITION("FMT_HEADER", addEscapedQuotes(srcDir + "fmt/include/"))
        .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_STATIC_LIB_DIRECTORY", addEscapedQuotes(configureDir + "0/hconfigure/"))
        .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_STATIC_LIB_PATH",
                                    addEscapedQuotes(configureDir + "0/hconfigure/hconfigure.lib"))
        .PRIVATE_COMPILE_DEFINITION("FMT_STATIC_LIB_DIRECTORY", addEscapedQuotes(configureDir + "0/fmt/"))
        .PRIVATE_COMPILE_DEFINITION("FMT_STATIC_LIB_PATH", addEscapedQuotes(configureDir + "0/fmt/fmt.lib"));

    DSC<CppSourceTarget> &hbuild =
        configuration.GetCppExeDSC("hbuild").PUBLIC_LIBRARIES(&hconfigure).PRIVATE_LIBRARIES(&stdhu);
    hbuild.getSourceTarget().MODULE_FILES("hbuild/src/main.cpp");
    configuration.setModuleScope(stdhu.getSourceTargetPointer());
}

void buildSpecification()
{
    Configuration &debug = GetConfiguration("Debug");
    Configuration &releaseSpeed = GetConfiguration("RSpeed");
    Configuration &releaseSize = GetConfiguration("RSize");
    Configuration &arm = GetConfiguration("arm");

    CxxSTD cxxStd = debug.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b;
    debug.ASSIGN(cxxStd, TreatModuleAsSource::NO, TranslateInclude::YES, ConfigType::DEBUG, AddressSanitizer::OFF,
                 RuntimeDebugging::OFF);
    debug.compilerFeatures.requirementCompileDefinitions.emplace("USE_HEADER_UNITS");
    releaseSpeed.ASSIGN(cxxStd, TreatModuleAsSource::YES, ConfigType::RELEASE);
    releaseSize.ASSIGN(cxxStd, TreatModuleAsSource::YES, ConfigType::RELEASE, Optimization::SPACE);
    arm.ASSIGN(cxxStd, Arch::ARM, TranslateInclude::YES, ConfigType::RELEASE, TreatModuleAsSource::NO);
    /*        debug.compilerFeatures.requirementCompilerFlags += "--target=x86_64-pc-windows-msvc ";
            debug.linkerFeatures.requirementLinkerFlags += "--target=x86_64-pc-windows-msvc";*/
    // configuration.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");

    for (const Configuration &configuration : targets<Configuration>)
    {
        configurationSpecification(const_cast<Configuration &>(configuration));
    }

    auto updateBTarget = [&](Builder &builder, unsigned short round) {
        for (LinkOrArchiveTarget *linkOrArchiveTarget : releaseSpeed.linkOrArchiveTargets)
        {
            std::filesystem::copy(linkOrArchiveTarget->getActualOutputPath(),
                                  sizeDifference.getSubDirForTarget() + linkOrArchiveTarget->actualOutputName,
                                  std::filesystem::copy_options::overwrite_existing);
        }
    };

    CTargetRoundZeroBTarget<void(Builder &, unsigned short), void(Builder &, unsigned short, unsigned int),
                            decltype(updateBTarget)>
        sizeDifference("Size-Difference");

    for (LinkOrArchiveTarget *linkOrArchiveTarget : releaseSpeed.linkOrArchiveTargets)
    {
        sizeDifference.getRealBTarget(0).addDependency(*linkOrArchiveTarget);
    }

    sizeDifference.setUpdateFunctor(lamb);
}

#ifdef EXE
int main(int argc, char **argv)
{
    try
    {
        initializeCache(getBuildSystemModeFromArguments(argc, argv));
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#else
extern "C" EXPORT int func2(BSMode bsMode_)
{
    try
    {
        exportAllSymbolsAndInitializeGlobals();
        initializeCache(bsMode_);
        buildSpecification();
        configureOrBuild();
    }
    catch (std::exception &ec)
    {
        string str(ec.what());
        if (!str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
#endif
