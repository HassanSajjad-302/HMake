#ifndef HMAKE_CONFIGURATION_HPP
#define HMAKE_CONFIGURATION_HPP
#ifdef USE_HEADER_UNITS
import "BasicTargets.hpp";
import "ConfigType.hpp";
import "DSC.hpp";
import <memory>;
#else
#include "BasicTargets.hpp"
#include "ConfigType.hpp"
#include "DSC.hpp"
#include <memory>
#endif

using std::shared_ptr;

class LinkOrArchiveTarget;
class CppSourceTarget;

enum class ConfigTargetHaveFile : bool
{
    YES,
    NO,
};

// TODO
// HollowConfiguration type which is very similar to Configuration except it does not inherit from CTarget which means
// CppSourceTarget etc could be created with a properties preset but a new Configuration Directory will not be created
struct Configuration : public CTarget
{
    vector<CppSourceTarget *> cppSourceTargets;
    vector<LinkOrArchiveTarget *> linkOrArchiveTargets;
    vector<PrebuiltLinkOrArchiveTarget *> prebuiltLinkOrArchiveTargets;
    vector<struct CSourceTarget *> prebuiltTargets;
    CppCompilerFeatures compilerFeatures;
    LinkerFeatures linkerFeatures;
    TargetType targetType = TargetType::LIBRARY_STATIC;
    bool archiving = false;

    CppSourceTarget &GetCppPreprocess(const pstring &name_);
    CppSourceTarget &GetCppObject(const pstring &name_);
    PrebuiltBasic &GetPrebuiltBasic(const pstring &name_);
    LinkOrArchiveTarget &GetExeLinkOrArchiveTarget(const pstring &name_);
    LinkOrArchiveTarget &GetStaticLinkOrArchiveTarget(const pstring &name_);
    LinkOrArchiveTarget &GetSharedLinkOrArchiveTarget(const pstring &name_);

    PrebuiltLinkOrArchiveTarget &GetPrebuiltLinkOrArchiveTarget(const pstring &name_, const pstring &directory,
                                                                TargetType linkTargetType_);
    PrebuiltLinkOrArchiveTarget &GetStaticPrebuiltLinkOrArchiveTarget(const pstring &name_, const pstring &directory);
    PrebuiltLinkOrArchiveTarget &GetSharedPrebuiltLinkOrArchiveTarget(const pstring &name_, const pstring &directory);

    CSourceTarget &GetCPT();

    DSC<CppSourceTarget> &GetCppExeDSC(const pstring &name_, bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &GetCppTargetDSC(const pstring &name_, TargetType targetType_ = cache.libraryType,
                                          bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &GetCppStaticDSC(const pstring &name_, bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &GetCppSharedDSC(const pstring &name_, bool defines = false, pstring define = "");
    DSC<CppSourceTarget> &GetCppObjectDSC(const pstring &name_, bool defines = false, pstring define = "");

    // _P means it will use PrebuiltLinkOrArchiveTarget instead of LinkOrArchiveTarget

    DSC<CppSourceTarget> &GetCppTargetDSC_P(const pstring &name_, const pstring &directory,
                                            TargetType targetType_ = cache.libraryType, bool defines = false,
                                            pstring define = "");
    DSC<CppSourceTarget> &GetCppTargetDSC_P(const pstring &name_, const pstring &prebuiltName, const pstring &directory,
                                            TargetType targetType_ = cache.libraryType, bool defines = false,
                                            pstring define = "");
    DSC<CppSourceTarget> &GetCppStaticDSC_P(const pstring &name_, const pstring &directory, bool defines = false,
                                            pstring define = "");

    DSC<CppSourceTarget> &GetCppSharedDSC_P(const pstring &name_, const pstring &directory, bool defines = false,
                                            pstring define = "");

    ConfigTargetHaveFile configTargetHaveFile = ConfigTargetHaveFile::YES;

    explicit Configuration(const pstring &name_);
    Configuration(const pstring &name, CTarget &other, bool hasFile = true);
    void markArchivePoint();
    // void setJson() override;
    C_Target *get_CAPITarget(BSMode bsModeLocal) override;
    template <typename T, typename... Property> Configuration &ASSIGN(T property, Property... properties);
};
bool operator<(const Configuration &lhs, const Configuration &rhs);

template <typename T, typename... Property> Configuration &Configuration::ASSIGN(T property, Property... properties)
{
    // ConfigTargetHaveFile property of Configuration class itself
    if constexpr (std::is_same_v<decltype(property), ConfigTargetHaveFile>)
    {
        configTargetHaveFile = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        targetType = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ConfigType>)
    {
        compilerFeatures.setConfigType(property);
        linkerFeatures.setConfigType(property);
    }
    // CommonFeatures
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
    else if constexpr (std::is_same_v<decltype(property), Define>)
    {
        compilerFeatures.requirementCompileDefinitions.emplace(property);
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
        return ASSIGN(properties...);
    }
    else
    {
        return *this;
    }
}

#endif // HMAKE_CONFIGURATION_HPP
