#ifndef HMAKE_CONFIGURATION_HPP
#define HMAKE_CONFIGURATION_HPP

#include "BasicTargets.hpp"
#include "ConfigType.hpp"
#include "Features.hpp"
#include <memory>
using std::shared_ptr;

class LinkOrArchiveTarget;
class CppSourceTarget;
enum class ConfigTargetHaveFile
{
    YES,
    NO,
};
struct Configuration : public CTarget
{
    set<CppSourceTarget> cppSourceTargets;
    set<LinkOrArchiveTarget> linkOrArchiveTargets;
    CommonFeatures commonFeatures;
    CompilerFeatures compilerFeatures;
    LinkerFeatures linkerFeatures;

    CppSourceTarget &GetPreProcessCpp(const string &name_);
    CppSourceTarget &GetCompileCpp(const string &name_);
    LinkOrArchiveTarget &GetExe(const string &name_);
    LinkOrArchiveTarget &GetLib(const string &name_);
    CppSourceTarget &GetExeCpp(const string &name_);
    LinkOrArchiveTarget &GetCppExe(const string &name_);
    CppSourceTarget &GetLibCpp(const string &name_);
    LinkOrArchiveTarget &GetCppLib(const string &name_);

    ConfigTargetHaveFile configTargetHaveFile = ConfigTargetHaveFile::YES;

    explicit Configuration(const string &name_);
    Configuration(const string &name, CTarget &other, bool hasFile = true);
    // Configuration()
    void setJson() override;
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Property>
    Configuration &ASSIGN(T property, Property... properties);
};

template <Dependency dependency, typename T, typename... Property>
Configuration &Configuration::ASSIGN(T property, Property... properties)
{
    // ConfigTargetHaveFile property of Configuration class itself
    if constexpr (std::is_same_v<decltype(property), ConfigTargetHaveFile>)
    {
        configTargetHaveFile = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ConfigType>)
    {
        compilerFeatures.setConfigType(property);
        commonFeatures.setConfigType(property);
    }
    // CommonFeatures
    else if constexpr (std::is_same_v<decltype(property), TargetOS>)
    {
        commonFeatures.targetOs = property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugSymbols>)
    {
        commonFeatures.debugSymbols = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Profiling>)
    {
        commonFeatures.profiling = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LocalVisibility>)
    {
        commonFeatures.localVisibility = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressSanitizer>)
    {
        commonFeatures.addressSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LeakSanitizer>)
    {
        commonFeatures.leakSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ThreadSanitizer>)
    {
        commonFeatures.threadSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), UndefinedSanitizer>)
    {
        commonFeatures.undefinedSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Coverage>)
    {
        commonFeatures.coverage = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTO>)
    {
        commonFeatures.lto = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeLink>)
    {
        commonFeatures.runtimeLink = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Arch>)
    {
        commonFeatures.arch = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressModel>)
    {
        commonFeatures.addModel = property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugStore>)
    {
        commonFeatures.debugStore = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeDebugging>)
    {
        commonFeatures.runtimeDebugging = property;
    }
    // CompilerFeatures
    else if constexpr (std::is_same_v<decltype(property), Compiler>)
    {
        compilerFeatures.compiler = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Threading>)
    {
        compilerFeatures.threading = property;
    }
    else if constexpr (std::is_same_v<decltype(property), enum Link>)
    {
        compilerFeatures.link = property;
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
    else if constexpr (std::is_same_v<decltype(property), LTOMode>)
    {
        linkerFeatures.ltoMode = property;
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
        return ASSIGN(properties...);
    }
    else
    {
        return *this;
    }
}

#endif // HMAKE_CONFIGURATION_HPP
