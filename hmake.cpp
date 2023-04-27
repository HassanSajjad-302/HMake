#include "Configure.hpp"

using std::filesystem::file_size;

void configurationSpecification(Configuration &configuration)
{
    DSC<CppSourceTarget> &stdhu = configuration.GetCppObjectDSC("stdhu");
    stdhu.getSourceTarget().assignStandardIncludesToHUIncludes();

    DSC<CppSourceTarget> &fmt = configuration.GetCppStaticDSC("fmt");
    fmt.getSourceTarget().MODULE_FILES("fmt/src/format.cc", "fmt/src/os.cc").PUBLIC_HU_INCLUDES("fmt/include");

    configuration.markArchivePoint();

    DSC<CppSourceTarget> &hconfigure = configuration.GetCppStaticDSC("hconfigure").PUBLIC_LIBRARIES(&fmt);
    hconfigure.getSourceTarget()
        .MODULE_DIRECTORIES("hconfigure/src/", ".*")
        .PUBLIC_HU_INCLUDES("hconfigure/header", "cxxopts/include", "json/include");

    DSC<CppSourceTarget> &hhelper = configuration.GetCppExeDSC("hhelper").PRIVATE_LIBRARIES(&hconfigure, &stdhu);
    hhelper.getSourceTarget()
        .MODULE_FILES("hhelper/src/main.cpp")
        .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_HEADER", addEscapedQuotes(srcDir + "hconfigure/header/"))
        .PRIVATE_COMPILE_DEFINITION("JSON_HEADER", addEscapedQuotes(srcDir + "json/include/"))
        .PRIVATE_COMPILE_DEFINITION("FMT_HEADER", addEscapedQuotes(srcDir + "fmt/include/"))
        .PRIVATE_COMPILE_DEFINITION(
            "HCONFIGURE_STATIC_LIB_DIRECTORY",
            addEscapedQuotes(
                path(hconfigure.linkOrArchiveTarget->getActualOutputPath()).parent_path().generic_string()))
        .PRIVATE_COMPILE_DEFINITION(
            "HCONFIGURE_STATIC_LIB_PATH",
            addEscapedQuotes(path(hconfigure.linkOrArchiveTarget->getActualOutputPath()).generic_string()))
        .PRIVATE_COMPILE_DEFINITION(
            "FMT_STATIC_LIB_DIRECTORY",
            addEscapedQuotes(path(fmt.linkOrArchiveTarget->getTargetFilePath()).parent_path().generic_string()))
        .PRIVATE_COMPILE_DEFINITION(
            "FMT_STATIC_LIB_PATH",
            addEscapedQuotes(path(fmt.linkOrArchiveTarget->getActualOutputPath()).generic_string()));

    DSC<CppSourceTarget> &hbuild = configuration.GetCppExeDSC("hbuild").PRIVATE_LIBRARIES(&hconfigure, &stdhu);
    hbuild.getSourceTarget().MODULE_FILES("hbuild/src/main.cpp");
    configuration.setModuleScope(stdhu.getSourceTargetPointer());

    DSC<CppSourceTarget> &hmakeHelper =
        configuration.GetCppExeDSC("HMakeHelper").PRIVATE_LIBRARIES(&hconfigure, &stdhu);
    hmakeHelper.getSourceTarget().MODULE_FILES("hmake.cpp").PRIVATE_COMPILE_DEFINITION("EXE");

    configuration.setModuleScope(stdhu.getSourceTargetPointer());
}

struct SizeDifference : public CTarget, public BTarget
{
    Configuration &sizeConfiguration;
    Configuration &speedConfiguration;

    SizeDifference(string name, Configuration &sizeConfiguration_, Configuration &speedConfiguration_)
        : CTarget(std::move(name)), sizeConfiguration(sizeConfiguration_), speedConfiguration(speedConfiguration_)
    {
        RealBTarget &realBTarget = getRealBTarget(0);
        if (speedConfiguration.getSelectiveBuild() && sizeConfiguration.getSelectiveBuild() && getSelectiveBuild())
        {
            realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            for (LinkOrArchiveTarget *linkOrArchiveTarget : sizeConfiguration.linkOrArchiveTargets)
            {
                if (linkOrArchiveTarget->EVALUATE(TargetType::EXECUTABLE))
                {
                    realBTarget.addDependency(*linkOrArchiveTarget);
                }
            }

            for (LinkOrArchiveTarget *linkOrArchiveTarget : speedConfiguration.linkOrArchiveTargets)
            {
                if (linkOrArchiveTarget->EVALUATE(TargetType::EXECUTABLE))
                {
                    realBTarget.addDependency(*linkOrArchiveTarget);
                }
            }
        }
    }

    virtual ~SizeDifference() = default;

    void updateBTarget(Builder &, unsigned short round) override
    {
        RealBTarget &realBTarget = getRealBTarget(0);

        if (!round && realBTarget.exitStatus == EXIT_SUCCESS && getSelectiveBuild())
        {

            string sizeDirPath = getSubDirForTarget() + "Size/";
            string speedDirPath = getSubDirForTarget() + "Speed/";

            std::filesystem::create_directories(sizeDirPath);
            std::filesystem::create_directories(speedDirPath);

            unsigned long long speedSize = 0;
            unsigned long long sizeSize = 0;
            for (LinkOrArchiveTarget *linkOrArchiveTarget : sizeConfiguration.linkOrArchiveTargets)
            {
                if (linkOrArchiveTarget->EVALUATE(TargetType::EXECUTABLE))
                {
                    sizeSize += file_size(linkOrArchiveTarget->getActualOutputPath());
                    std::filesystem::copy(linkOrArchiveTarget->getActualOutputPath(),
                                          sizeDirPath + linkOrArchiveTarget->actualOutputName,
                                          std::filesystem::copy_options::overwrite_existing);
                }
            }

            for (LinkOrArchiveTarget *linkOrArchiveTarget : speedConfiguration.linkOrArchiveTargets)
            {
                if (linkOrArchiveTarget->EVALUATE(TargetType::EXECUTABLE))
                {
                    speedSize += file_size(linkOrArchiveTarget->getActualOutputPath());
                    std::filesystem::copy(linkOrArchiveTarget->getActualOutputPath(),
                                          speedDirPath + linkOrArchiveTarget->actualOutputName,
                                          std::filesystem::copy_options::overwrite_existing);
                }
            }

            std::lock_guard<mutex> lk{printMutex};
            fmt::print("Speed build size - {}\nSize build size - {}\n", speedSize, sizeSize);
            fflush(stdout);
        }
    }
};

bool operator<(const SizeDifference &lhs, const SizeDifference &rhs)
{
    return lhs.CTarget::id < rhs.CTarget::id;
}

void buildSpecification()
{
    // Configuration &debug = GetConfiguration("Debug");
    Configuration &releaseSpeed = GetConfiguration("RSpeed");
    Configuration &releaseSize = GetConfiguration("RSize");
    //  Configuration &arm = GetConfiguration("arm");

    CxxSTD cxxStd = releaseSpeed.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b;
    /*    debug.ASSIGN(cxxStd, TreatModuleAsSource::NO, TranslateInclude::YES, ConfigType::DEBUG, AddressSanitizer::OFF,
                     RuntimeDebugging::OFF);
        debug.compilerFeatures.requirementCompileDefinitions.emplace("USE_HEADER_UNITS");*/
    releaseSpeed.ASSIGN(cxxStd, TreatModuleAsSource::YES, ConfigType::RELEASE);
    releaseSize.ASSIGN(cxxStd, TreatModuleAsSource::YES, ConfigType::RELEASE, Optimization::SPACE);
    //  arm.ASSIGN(cxxStd, Arch::ARM, TranslateInclude::YES, ConfigType::RELEASE, TreatModuleAsSource::NO);
    /*        debug.compilerFeatures.requirementCompilerFlags += "--target=x86_64-pc-windows-msvc ";
            debug.linkerFeatures.requirementLinkerFlags += "--target=x86_64-pc-windows-msvc";*/
    // configuration.privateCompileDefinitions.emplace_back("USE_HEADER_UNITS", "1");

    for (const Configuration &configuration : targets<Configuration>)
    {
        if (const_cast<Configuration &>(configuration).getSelectiveBuild())
        {
            configurationSpecification(const_cast<Configuration &>(configuration));
        }
    }

    targets<SizeDifference>.emplace("Size-Difference", releaseSize, releaseSpeed);
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
