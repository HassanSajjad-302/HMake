#include "Configure.hpp"

using std::filesystem::file_size;

struct SizeDifference : public CTarget, public BTarget
{
    Configuration &sizeConfiguration;
    Configuration &speedConfiguration;

    SizeDifference(string name, Configuration &sizeConfiguration_, Configuration &speedConfiguration_)
        : CTarget(std::move(name)), sizeConfiguration(sizeConfiguration_), speedConfiguration(speedConfiguration_)
    {
        RealBTarget &realBTarget = realBTargets[0];
        if (speedConfiguration.getSelectiveBuild() && sizeConfiguration.getSelectiveBuild() && getSelectiveBuild())
        {
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

    BTarget *getBTarget() override
    {
        return this;
    }

    virtual ~SizeDifference() = default;

    void updateBTarget(Builder &, unsigned short round) override
    {
        RealBTarget &realBTarget = realBTargets[0];

        if (!round && realBTarget.exitStatus == EXIT_SUCCESS)
        {
            string sizeDirPath = targetSubDir + "Size";
            string speedDirPath = targetSubDir + "Speed";

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

void configurationSpecification(Configuration &configuration)
{
    DSC<CppSourceTarget> &stdhu = configuration.GetCppObjectDSC("stdhu");
    stdhu.getSourceTarget().assignStandardIncludesToPublicHUDirectories();

    DSC<CppSourceTarget> &fmt = configuration.GetCppStaticDSC("fmt").PUBLIC_LIBRARIES(&stdhu);
    fmt.getSourceTarget().MODULE_FILES("fmt/src/format.cc", "fmt/src/os.cc").PUBLIC_HU_INCLUDES("fmt/include");

    configuration.markArchivePoint();

    DSC<CppSourceTarget> &hconfigure = configuration.GetCppStaticDSC("hconfigure").PUBLIC_LIBRARIES(&fmt);
    hconfigure.getSourceTarget()
        .MODULE_DIRECTORIES("hconfigure/src")
        .PUBLIC_HU_INCLUDES("hconfigure/header", "cxxopts/include", "json/include", "rapidjson/include");

    DSC<CppSourceTarget> &hhelper = configuration.GetCppExeDSC("hhelper").PRIVATE_LIBRARIES(&hconfigure, &stdhu);
    hhelper.getSourceTarget()
        .MODULE_FILES("hhelper/src/main.cpp")
        .PRIVATE_COMPILE_DEFINITION("HCONFIGURE_HEADER", addEscapedQuotes(srcDir + "hconfigure/header"))
        .PRIVATE_COMPILE_DEFINITION("JSON_HEADER", addEscapedQuotes(srcDir + "json/include"))
        .PRIVATE_COMPILE_DEFINITION("RAPIDJSON_HEADER", addEscapedQuotes(srcDir + "rapidjson/include"))
        .PRIVATE_COMPILE_DEFINITION("FMT_HEADER", addEscapedQuotes(srcDir + "fmt/include"))
        .PRIVATE_COMPILE_DEFINITION(
            "HCONFIGURE_STATIC_LIB_DIRECTORY",
            addEscapedQuotes(path(hconfigure.getLinkOrArchiveTarget().getActualOutputPath()).parent_path().string()))
        .PRIVATE_COMPILE_DEFINITION(
            "HCONFIGURE_STATIC_LIB_PATH",
            addEscapedQuotes(path(hconfigure.getLinkOrArchiveTarget().getActualOutputPath()).string()))
        .PRIVATE_COMPILE_DEFINITION(
            "FMT_STATIC_LIB_DIRECTORY",
            addEscapedQuotes(path(fmt.getLinkOrArchiveTarget().getActualOutputPath()).parent_path().string()))
        .PRIVATE_COMPILE_DEFINITION(
            "FMT_STATIC_LIB_PATH", addEscapedQuotes(path(fmt.getLinkOrArchiveTarget().getActualOutputPath()).string()));

    DSC<CppSourceTarget> &hbuild = configuration.GetCppExeDSC("hbuild").PRIVATE_LIBRARIES(&hconfigure, &stdhu);
    hbuild.getSourceTarget().MODULE_FILES("hbuild/src/main.cpp");

    DSC<CppSourceTarget> &hmakeHelper =
        configuration.GetCppExeDSC("HMakeHelper").PRIVATE_LIBRARIES(&hconfigure, &stdhu);
    hmakeHelper.getSourceTarget().MODULE_FILES("hmake.cpp").PRIVATE_COMPILE_DEFINITION("EXE");
}

void buildSpecification()
{
    Configuration &releaseSpeed = GetConfiguration("RSpeed");
    CxxSTD cxxStd = releaseSpeed.compilerFeatures.compiler.bTFamily == BTFamily::MSVC ? CxxSTD::V_LATEST : CxxSTD::V_2b;
    releaseSpeed.ASSIGN(cxxStd, TreatModuleAsSource::YES, TranslateInclude::YES, ConfigType::RELEASE);
    // releaseSpeed.compilerFeatures.requirementCompileDefinitions.emplace("USE_HEADER_UNITS", "1");

    Configuration &releaseSize = GetConfiguration("RSize");
    releaseSize.ASSIGN(cxxStd, TreatModuleAsSource::YES, ConfigType::RELEASE, Optimization::SPACE);

    if (selectiveConfigurationSpecification(&configurationSpecification))
    {
        targets<SizeDifference>.emplace("Size-Difference", releaseSize, releaseSpeed);
    }
}

MAIN_FUNCTION