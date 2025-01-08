#ifndef HMAKE_CONFIGURATION_HPP
#define HMAKE_CONFIGURATION_HPP
#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
import "ConfigType.hpp";
import "DSC.hpp";
import "LinkOrArchiveTarget.hpp";
import <memory>;
#else
#include "BTarget.hpp"
#include "ConfigType.hpp"
#include "DSC.hpp"
#include "LinkOrArchiveTarget.hpp"
#include <memory>
#endif

using std::shared_ptr;

class CppSourceTarget;

enum class GenerateModuleData : char
{
    NO,
    YES,
};

enum class UseModuleData : char
{
    NO,
    YES,
};

enum class AssignStandardCppTarget : char
{
    NO,
    YES,
};

enum class BuildTests : char
{
    NO,
    YES,
};

enum class BuildExamples : char
{
    NO,
    YES,
};

enum class TestsExplicit : char
{
    NO,
    YES,
};

enum class ExamplesExplicit : char
{
    NO,
    YES,
};

enum class BuildTestsExplicitBuild : char
{
    NO,
    YES,
};

enum class BuildExamplesExplicitBuild : char
{
    NO,
    YES,
};

enum class BuildTestsAndExamples : char
{
    NO,
    YES,
};

enum class BuildTestsAndExamplesExplicitBuild : char
{
    NO,
    YES,
};

struct Configuration : BTarget
{
    vector<CppSourceTarget *> cppSourceTargets;
    vector<LinkOrArchiveTarget *> linkOrArchiveTargets;
    vector<PrebuiltLinkOrArchiveTarget *> prebuiltLinkOrArchiveTargets;
    vector<CSourceTarget *> prebuiltTargets;
    CppCompilerFeatures compilerFeatures;
    PrebuiltLinkerFeatures prebuiltLinkOrArchiveTargetFeatures;
    PrebuiltBasicFeatures prebuiltBasicFeatures;
    LinkerFeatures linkerFeatures;
    TargetType targetType = TargetType::LIBRARY_STATIC;
    GenerateModuleData generateModuleData = GenerateModuleData::NO;
    AssignStandardCppTarget assignStandardCppTarget = AssignStandardCppTarget::NO;
    BuildTests buildTests = BuildTests::NO;
    BuildExamples buildExamples = BuildExamples::NO;
    TestsExplicit testsExplicit = TestsExplicit::NO;
    ExamplesExplicit examplesExplicit = ExamplesExplicit::NO;

    DSC<CppSourceTarget> *stdCppTarget = nullptr;
    bool archiving = false;

    CppSourceTarget &getCppPreprocess(const pstring &name_);
    // TODO
    // Need overloads which do not add configuration name
    CppSourceTarget &getCppObject(const pstring &name_);
    CppSourceTarget &getCppObject(bool explicitBuild, const pstring &buildCacheFilesDirPath_, const pstring &name_);
    PrebuiltBasic &getPrebuiltBasic(const pstring &name_) const;
    LinkOrArchiveTarget &GetExeLinkOrArchiveTarget(const pstring &name_);
    LinkOrArchiveTarget &GetExeLinkOrArchiveTarget(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                   const pstring &name_);
    LinkOrArchiveTarget &getStaticLinkOrArchiveTarget(const pstring &name_);
    LinkOrArchiveTarget &getStaticLinkOrArchiveTarget(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                      const pstring &name_);
    LinkOrArchiveTarget &getSharedLinkOrArchiveTarget(const pstring &name_);
    LinkOrArchiveTarget &getSharedLinkOrArchiveTarget(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                      const pstring &name_);

    PrebuiltLinkOrArchiveTarget &getPrebuiltLinkOrArchiveTarget(const pstring &name_, const pstring &directory,
                                                                TargetType linkTargetType_);
    PrebuiltLinkOrArchiveTarget &getStaticPrebuiltLinkOrArchiveTarget(const pstring &name_, const pstring &directory);
    PrebuiltLinkOrArchiveTarget &getSharedPrebuiltLinkOrArchiveTarget(const pstring &name_, const pstring &directory);
    DSC<CppSourceTarget> &addStdCppDep(DSC<CppSourceTarget> &target);

    // CSourceTarget &GetCPT();

    DSC<CppSourceTarget> &getCppExeDSC(const pstring &name_, bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppExeDSC(bool explicitBuild, const pstring &buildCacheFilesDirPath_, const pstring &name_,
                                       bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppTargetDSC(const pstring &name_, TargetType targetType_ = cache.libraryType,
                                          bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppTargetDSC(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                          const pstring &name_, TargetType targetType_ = cache.libraryType,
                                          bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppStaticDSC(const pstring &name_, bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppStaticDSC(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                          const pstring &name_, bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppSharedDSC(const pstring &name_, bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppSharedDSC(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                          const pstring &name_, bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppObjectDSC(const pstring &name_, bool defines = false, pstring define = "");

    DSC<CppSourceTarget> &getCppObjectDSC(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                          const pstring &name_, bool defines = false, pstring define = "");

    // _P means it will use PrebuiltLinkOrArchiveTarget instead of LinkOrArchiveTarget

    DSC<CppSourceTarget> &getCppTargetDSC_P(const pstring &name_, const pstring &directory,
                                            TargetType targetType_ = cache.libraryType, bool defines = false,
                                            pstring define = "");
    DSC<CppSourceTarget> &getCppTargetDSC_P(const pstring &name_, const pstring &prebuiltName, const pstring &directory,
                                            TargetType targetType_ = cache.libraryType, bool defines = false,
                                            pstring define = "");
    DSC<CppSourceTarget> &getCppStaticDSC_P(const pstring &name_, const pstring &directory, bool defines = false,
                                            pstring define = "");

    DSC<CppSourceTarget> &getCppSharedDSC_P(const pstring &name_, const pstring &directory, bool defines = false,
                                            pstring define = "");

    // These NoName functions do not prepend configuration name to the target name.
    CppSourceTarget &getCppPreprocessNoName(const pstring &name_);
    // TODO
    // Need overloads which do not add configuration name
    CppSourceTarget &getCppObjectNoName(const pstring &name_);
    CppSourceTarget &getCppObjectNoName(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                        const pstring &name_);
    PrebuiltBasic &getPrebuiltBasicNoName(const pstring &name_) const;
    LinkOrArchiveTarget &GetExeLinkOrArchiveTargetNoName(const pstring &name_);
    LinkOrArchiveTarget &GetExeLinkOrArchiveTargetNoName(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                         const pstring &name_);
    LinkOrArchiveTarget &getStaticLinkOrArchiveTargetNoName(const pstring &name_);
    LinkOrArchiveTarget &getStaticLinkOrArchiveTargetNoName(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                            const pstring &name_);
    LinkOrArchiveTarget &getSharedLinkOrArchiveTargetNoName(const pstring &name_);
    LinkOrArchiveTarget &getSharedLinkOrArchiveTargetNoName(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                            const pstring &name_);

    PrebuiltLinkOrArchiveTarget &getPrebuiltLinkOrArchiveTargetNoName(const pstring &name_, const pstring &directory,
                                                                      TargetType linkTargetType_);
    PrebuiltLinkOrArchiveTarget &getStaticPrebuiltLinkOrArchiveTargetNoName(const pstring &name_,
                                                                            const pstring &directory);
    PrebuiltLinkOrArchiveTarget &getSharedPrebuiltLinkOrArchiveTargetNoName(const pstring &name_,
                                                                            const pstring &directory);
    DSC<CppSourceTarget> &addStdCppDepNoName(DSC<CppSourceTarget> &target);

    // CSourceTarget &GetCPTNoName();

    DSC<CppSourceTarget> &getCppExeDSCNoName(const pstring &name_, bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppExeDSCNoName(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                             const pstring &name_, bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppTargetDSCNoName(const pstring &name_, TargetType targetType_ = cache.libraryType,
                                                bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppTargetDSCNoName(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                const pstring &name_, TargetType targetType_ = cache.libraryType,
                                                bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppStaticDSCNoName(const pstring &name_, bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppStaticDSCNoName(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                const pstring &name_, bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppSharedDSCNoName(const pstring &name_, bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppSharedDSCNoName(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                const pstring &name_, bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppObjectDSCNoName(const pstring &name_, bool defines = false, pstring define = "");

    DSC<CppSourceTarget> &getCppObjectDSCNoName(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                const pstring &name_, bool defines = false, pstring define = "");

    // _P means it will use PrebuiltLinkOrArchiveTarget instead of LinkOrArchiveTarget

    DSC<CppSourceTarget> &getCppTargetDSC_PNoName(const pstring &name_, const pstring &directory,
                                                  TargetType targetType_ = cache.libraryType, bool defines = false,
                                                  pstring define = "");
    DSC<CppSourceTarget> &getCppTargetDSC_PNoName(const pstring &name_, const pstring &prebuiltName,
                                                  const pstring &directory, TargetType targetType_ = cache.libraryType,
                                                  bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &getCppStaticDSC_PNoName(const pstring &name_, const pstring &directory, bool defines = false,
                                                  pstring define = "");

    DSC<CppSourceTarget> &getCppSharedDSC_PNoName(const pstring &name_, const pstring &directory, bool defines = false,
                                                  pstring define = "");

    explicit Configuration(const pstring &name_);
    static void markArchivePoint();
    template <typename T, typename... Property> Configuration &assign(T property, Property... properties);
    template <typename T> bool evaluate(T property) const;
};
bool operator<(const Configuration &lhs, const Configuration &rhs);

template <typename T, typename... Property> Configuration &Configuration::assign(T property, Property... properties)
{
    if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        targetType = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ConfigType>)
    {
        compilerFeatures.setConfigType(property);
        linkerFeatures.setConfigType(property);
    }
    else if constexpr (std::is_same_v<decltype(property), GenerateModuleData>)
    {
        generateModuleData = property;
        if (generateModuleData == GenerateModuleData::YES)
        {
            assign(TreatModuleAsSource::YES);
        }
    }
    else if constexpr (std::is_same_v<decltype(property), DSC<CppSourceTarget> *>)
    {
        stdCppTarget = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AssignStandardCppTarget>)
    {
        assignStandardCppTarget = property;
    }
    else if constexpr (std::is_same_v<decltype(property), BuildTests>)
    {
        buildTests = property;
    }
    else if constexpr (std::is_same_v<decltype(property), BuildExamples>)
    {
        buildExamples = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TestsExplicit>)
    {
        testsExplicit = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExamplesExplicit>)
    {
        examplesExplicit = property;
    }
    else if constexpr (std::is_same_v<decltype(property), BuildTestsExplicitBuild>)
    {
        if (property == BuildTestsExplicitBuild::YES)
        {
            buildTests = BuildTests::YES;
            testsExplicit = TestsExplicit::YES;
        }
        else
        {
            buildTests = BuildTests::NO;
        }
    }
    else if constexpr (std::is_same_v<decltype(property), BuildExamplesExplicitBuild>)
    {
        if (property == BuildExamplesExplicitBuild::YES)
        {
            buildExamples = BuildExamples::YES;
            examplesExplicit = ExamplesExplicit::YES;
        }
        else
        {
            buildExamples = BuildExamples::NO;
        }
    }
    else if constexpr (std::is_same_v<decltype(property), BuildTestsAndExamples>)
    {
        if (property == BuildTestsAndExamples::YES)
        {
            buildTests = BuildTests::YES;
            buildExamples = BuildExamples::YES;
        }
        else
        {
            buildTests = BuildTests::NO;
            buildExamples = BuildExamples::NO;
        }
    }
    else if constexpr (std::is_same_v<decltype(property), BuildTestsAndExamplesExplicitBuild>)
    {
        if (property == BuildTestsAndExamplesExplicitBuild::YES)
        {
            buildTests = BuildTests::YES;
            buildExamples = BuildExamples::YES;
            testsExplicit = TestsExplicit::YES;
            examplesExplicit = ExamplesExplicit::YES;
        }
        else
        {
            buildTests = BuildTests::NO;
            buildExamples = BuildExamples::NO;
        }
    }
    // CommonFeatures
    else if constexpr (std::is_same_v<decltype(property), UseMiniTarget>)
    {
        prebuiltBasicFeatures.useMiniTarget = property;
        useMiniTarget = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetOS>)
    {
        compilerFeatures.targetOs = property;
        linkerFeatures.targetOs = property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugSymbols>)
    {
        compilerFeatures.debugSymbols = property;
        linkerFeatures.debugSymbols = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Profiling>)
    {
        compilerFeatures.profiling = property;
        linkerFeatures.profiling = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Visibility>)
    {
        compilerFeatures.localVisibility = property;
        linkerFeatures.visibility = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressSanitizer>)
    {
        compilerFeatures.addressSanitizer = property;
        linkerFeatures.addressSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LeakSanitizer>)
    {
        compilerFeatures.leakSanitizer = property;
        linkerFeatures.leakSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ThreadSanitizer>)
    {
        compilerFeatures.threadSanitizer = property;
        linkerFeatures.threadSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), UndefinedSanitizer>)
    {
        compilerFeatures.undefinedSanitizer = property;
        linkerFeatures.undefinedSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Coverage>)
    {
        compilerFeatures.coverage = property;
        linkerFeatures.coverage = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTO>)
    {
        compilerFeatures.lto = property;
        linkerFeatures.lto = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTOMode>)
    {
        compilerFeatures.ltoMode = property;
        linkerFeatures.ltoMode = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeLink>)
    {
        compilerFeatures.runtimeLink = property;
        linkerFeatures.runtimeLink = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Arch>)
    {
        compilerFeatures.arch = property;
        linkerFeatures.arch = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressModel>)
    {
        compilerFeatures.addModel = property;
        linkerFeatures.addModel = property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugStore>)
    {
        compilerFeatures.debugStore = property;
        linkerFeatures.debugStore = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeDebugging>)
    {
        compilerFeatures.runtimeDebugging = property;
        linkerFeatures.runtimeDebugging = property;
    }
    // CppCompilerFeatures
    else if constexpr (std::is_same_v<decltype(property), CxxFlags>)
    {
        compilerFeatures.requirementCompilerFlags += property;
    }
    else if constexpr (std::is_same_v<decltype(property), Define>)
    {
        compilerFeatures.requirementCompileDefinitions.emplace(property);
    }
    else if constexpr (std::is_same_v<decltype(property), Compiler>)
    {
        compilerFeatures.compiler = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Threading>)
    {
        compilerFeatures.threading = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTD>)
    {
        compilerFeatures.cxxStd = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTDDialect>)
    {
        compilerFeatures.cxxStdDialect = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Optimization>)
    {
        compilerFeatures.optimization = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Inlining>)
    {
        compilerFeatures.inlining = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Warnings>)
    {
        compilerFeatures.warnings = property;
    }
    else if constexpr (std::is_same_v<decltype(property), WarningsAsErrors>)
    {
        compilerFeatures.warningsAsErrors = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExceptionHandling>)
    {
        compilerFeatures.exceptionHandling = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AsyncExceptions>)
    {
        compilerFeatures.asyncExceptions = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RTTI>)
    {
        compilerFeatures.rtti = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExternCNoThrow>)
    {
        compilerFeatures.externCNoThrow = property;
    }
    else if constexpr (std::is_same_v<decltype(property), StdLib>)
    {
        compilerFeatures.stdLib = property;
    }
    else if constexpr (std::is_same_v<decltype(property), InstructionSet>)
    {
        compilerFeatures.instructionSet = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CpuType>)
    {
        compilerFeatures.cpuType = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TranslateInclude>)
    {
        compilerFeatures.translateInclude = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TreatModuleAsSource>)
    {
        compilerFeatures.treatModuleAsSource = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Define>)
    {
        compilerFeatures.requirementCompileDefinitions.emplace(property);
    }
    else if constexpr (std::is_same_v<decltype(property), CopyDLLToExeDirOnNTOs>)
    {
        prebuiltLinkOrArchiveTargetFeatures.copyToExeDirOnNtOs = property;
    }
    // Linker Features
    else if constexpr (std::is_same_v<decltype(property), LinkFlags>)
    {
        linkerFeatures.requirementLinkerFlags += property;
    }
    else if constexpr (std::is_same_v<decltype(property), UserInterface>)
    {
        linkerFeatures.userInterface = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Strip>)
    {
        linkerFeatures.strip = property;
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        property;
    }
    else
    {
        linkerFeatures.strip = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
    if constexpr (sizeof...(properties))
    {
        return assign(properties...);
    }
    else
    {
        return *this;
    }
}

template <typename T> bool Configuration::evaluate(T property) const
{
    if constexpr (std::is_same_v<decltype(property), UseMiniTarget>)
    {
        return useMiniTarget == property;
    }
    else if constexpr (std::is_same_v<decltype(property), GenerateModuleData>)
    {
        return generateModuleData == property;
    }
    else if constexpr (std::is_same_v<decltype(property), DSC<CppSourceTarget> *>)
    {
        return stdCppTarget == property;
    }
    else if constexpr (std::is_same_v<decltype(property), AssignStandardCppTarget>)
    {
        return assignStandardCppTarget == property;
    }
    else if constexpr (std::is_same_v<decltype(property), BuildTests>)
    {
        return buildTests == property;
    }
    else if constexpr (std::is_same_v<decltype(property), BuildExamples>)
    {
        return buildExamples == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TestsExplicit>)
    {
        return testsExplicit == property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExamplesExplicit>)
    {
        return examplesExplicit == property;
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        return property;
    }
    else
    {
        useMiniTarget = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
}

#endif // HMAKE_CONFIGURATION_HPP
