#ifndef HMAKE_FEATURES_HPP
#define HMAKE_FEATURES_HPP

/**
 * @file Features.hpp
 * @brief Strongly typed compiler, linker, and target properties.
 *
 * A normal `hmake.cpp` applies these values through `Configuration::assign()`:
 *
 * @code{.cpp}
 * getConfiguration("Debug").assign(ConfigType::DEBUG, CxxSTD::V_23,
 *                                    Warnings::EXTRA, WarningsAsErrors::ON);
 * @endcode
 *
 * Variadic assignments are processed from left to right. This matters for preset properties:
 * put `ConfigType` first and any deliberate optimization/debug overrides after it. The member
 * initializers in the feature structs below are the authoritative defaults; an enum's first
 * enumerator is not necessarily its default.
 */

#include "BuildTools.hpp"
#include "Cache.hpp"

#include "TargetType.hpp"
#include <vector>

using std::vector;

class Node;

/// Selects the C-source flavor when the compiler feature set is shared with a C target.
enum class CSourceTargetEnum
{
    NO,
    YES
};

/// Controls the default copy of runtime DLL dependencies beside executables on NT-family systems.
enum class CopyDLLToExeDirOnNTOs : bool
{
    NO,
    YES
};

/// Controls whether a shared-library export definition is present while compiling the library itself.
enum class DefineDLLPrivate : bool
{
    YES,
    NO,
};

/// Controls whether a shared-library import definition is propagated to consumers.
enum class DefineDLLInterface : bool
{
    YES,
    NO,
};

// The feature vocabulary is inspired by Boost.Build. Optional properties use explicit OFF/NONE values so that every
// feature has a concrete state and can be compared with evaluate(). Defaults are declared on the feature structs.

/// @name Platform and ABI features
/// @{

enum class Arch : uint8_t // Architecture
{
    X86,
    ARM,
    S390X,
    POWER,
    LOONGARCH,
    NONE,
};


enum class AddressModel : uint8_t // AddressModel
{
    A_32,
    A_64,
    NONE,
};


/// Raw compiler flags appended to a target through `Configuration::assign()`.
struct CxxFlags : string
{
};

/// Requests a compiler-specific maximum template instantiation depth.
struct TemplateDepth
{
    unsigned long long templateDepth;
    explicit TemplateDepth(unsigned long long templateDepth_);
};

/// A preprocessor definition. An empty value emits a name-only definition.
struct Define
{
    string name;
    string value;
    Define() = default;
    explicit Define(string name_, string value_ = "");
};
/// @}

/// @name Diagnostics, runtime, and code-generation features
/// @{
enum class Threading : bool
{
    SINGLE,
    MULTI
};

enum class Warnings : uint8_t
{
    ON,
    ALL,
    EXTRA,
    PEDANTIC,
    OFF,
};

enum class WarningsAsErrors : bool
{
    OFF,
    ON,
};

enum class ExceptionHandling : bool
{
    ON,
    OFF
};

enum class AsyncExceptions : bool
{
    OFF,
    ON,
};

enum class ExternCNoThrow : bool
{
    OFF,
    ON,
};

enum class RTTI : bool
{
    ON,
    OFF,
};

enum class DebugSymbols : bool
{
    ON,
    OFF,
};

enum class Profiling : bool
{
    OFF,
    ON,
};

enum class Visibility : uint8_t
{
    OFF,
    GLOBAL,
    HIDDEN,
    PROTECTED,
};

/// Broad build presets. Later assignments can override individual values selected by a preset.
enum class ConfigType : uint8_t
{
    DEBUG,
    RELEASE,
    PROFILE,
    NONE,
};

enum class AddressSanitizer : uint8_t
{
    OFF,
    NORECOVER,
    ON,
};

enum class LeakSanitizer : uint8_t
{
    OFF,
    NORECOVER,
    ON,
};

enum class ThreadSanitizer : uint8_t
{
    OFF,
    NORECOVER,
    ON,
};

enum class UndefinedSanitizer : uint8_t
{
    OFF,
    NORECOVER,
    ON,
};

enum class LTO : bool
{
    OFF,
    ON,
};

enum class LTOMode : uint8_t
{
    FAT,
    FULL,
    THIN,
};

enum class StdLib : uint8_t
{
    NATIVE,
    GNU,
    GNU11,
    LIBCPP, // libc++
};

enum class Coverage : bool
{
    OFF,
    ON,
};

enum class RuntimeLink : bool
{
    SHARED,
    STATIC,
};

enum class RuntimeDebugging : bool
{
    OFF,
    ON
};
/// @}

/// Maps a logical target name to the platform-specific file name (for example, `libname.a`).
string getActualNameFromTargetName(TargetType targetType, enum OS osLocal, const string &targetName);
/// Recovers HMake's logical target name from a platform-specific artifact name.
string getTargetNameFromActualName(TargetType targetType, enum OS osLocal, const string &actualName);
/// Returns the platform-appropriate command path for an executable (`./name` or `name.exe`).
string getSlashedExecutableName(const string &name);

/// @name Language and optimization features
/// @{
/// C++ language-standard spellings supported by HMake's compiler adapters.
enum class CxxSTD : uint8_t
{
    V_98,
    V_03,
    V_0x,
    V_11,
    V_1y,
    V_14,
    V_1z,
    V_17,
    V_2a,
    V_20,
    V_2b,
    V_23,
    V_2c,
    V_26,
    V_LATEST,
};

enum class CxxSTDDialect : uint8_t
{
    ISO,
    GNU,
    MS,
};

enum class TargetOS : uint8_t
{
    ANDROID,
    APPLETV,
    CYGWIN,
    DARWIN,
    FREEBSD,
    IPHONE,
    LINUX_,
    OPENBSD,
    QNX,
    WINDOWS,
    NONE,
};

enum class Language : uint8_t
{
    C,
    CPP,
    OBJECTIVE_C,
    OBJECTIVE_CPP,
};

enum class Optimization : uint8_t
{
    OFF,
    SPEED,
    SPACE,
    MINIMAL,
    DEBUG,
};

enum class Inlining : uint8_t
{
    OFF,
    ON,
    FULL,
};

enum class Vectorize : uint8_t
{
    OFF,
    ON,
    FULL,
};

enum class UserInterface : uint8_t
{
    CONSOLE,
    GUI,
    WINCE,
    NATIVE,
    AUTO,
};

enum class Strip : bool
{
    OFF,
    ON,
};

enum class InstructionSet : unsigned short
{
    OFF,
    native,
    x86_64_v1,
    x86_64_v2,
    x86_64_v3,
    x86_64_v4,
};

// Declared on Line 2143 in msvc.jam
enum class CpuType : uint8_t
{
    AMD64,
    ARM,
    NONE,
};

// Declared on Line 1871 msvc.jam
enum class DebugStore : bool
{
    OBJECT,
    DATABASE,
};
/// @}

/// Export/import-definition settings needed by a prebuilt side of a DSC.
struct DSCPrebuiltFeatures
{
    DefineDLLInterface defineDllInterface = DefineDLLInterface::NO;
};

/// Export/import-definition settings for a DSC that compiles its own sources.
struct DSCFeatures : DSCPrebuiltFeatures
{
    DefineDLLPrivate defineDllPrivate = DefineDLLPrivate::NO;
};

/// Link behavior that applies specifically when consuming prebuilt artifacts.
struct PrebuiltLinkerFeatures
{
    CopyDLLToExeDirOnNTOs copyToExeDirOnNtOs = CopyDLLToExeDirOnNTOs::YES;

    /// Returns whether the supplied prebuilt-link property is currently selected.
    template <typename T> bool evaluate(T property) const;
};

template <typename T> bool PrebuiltLinkerFeatures::evaluate(T property) const
{
    if constexpr (std::is_same_v<decltype(property), CopyDLLToExeDirOnNTOs>)
    {
        return copyToExeDirOnNtOs == property;
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        return property;
    }
    else
    {
        static_assert(false && "No property matched in PrebuiltLinkerFeatures::evaluate\n");
    }
}



/**
 * @brief Typed settings used to construct linker and archiver command lines.
 *
 * Users usually update this object indirectly through `Configuration::assign()`, which
 * keeps compile and link properties such as sanitizers, LTO, and runtime mode in sync.
 * Direct assignment is useful for advanced integrations that own a standalone link step.
 */
struct LinkerFeatures
{
    AddressSanitizer addressSanitizer = AddressSanitizer::OFF;
    LeakSanitizer leakSanitizer = LeakSanitizer::OFF;
    ThreadSanitizer threadSanitizer = ThreadSanitizer::OFF;
    UndefinedSanitizer undefinedSanitizer = UndefinedSanitizer::OFF;

    Coverage coverage = Coverage::OFF;
    LTO lto = LTO::OFF;
    LTOMode ltoMode = LTOMode::FULL;
    RuntimeLink runtimeLink = RuntimeLink::SHARED;
    RuntimeDebugging runtimeDebugging = RuntimeDebugging::ON;
    TargetOS targetOs;
    DebugSymbols debugSymbols = DebugSymbols::ON;
    Profiling profiling = Profiling::OFF;
    Visibility visibility = Visibility::HIDDEN;

    ConfigType configurationType;

    // Following two are initialized in constructor
    // AddressModel and Architecture to target for.
    Arch arch;
    AddressModel addModel;

    // Windows Specifc
    DebugStore debugStore = DebugStore::OBJECT;

    Strip strip = Strip::OFF;

    // Windows specific
    UserInterface userInterface = UserInterface::CONSOLE;
    InstructionSet instructionSet = InstructionSet::OFF;
    CpuType cpuType;

    CxxSTD cxxStd = CxxSTD::V_LATEST;
    CxxSTDDialect cxxStdDialect = CxxSTDDialect::ISO;
    Linker linker;
    Archiver archiver;
    // In threading-feature.jam the default value is single, but author here prefers multi
    Threading threading = Threading::MULTI;

    TargetType libraryType;
    LinkerFeatures();

    /// Produces flags for the selected linker and current feature values.
    string getLinkerFlags();

    /// Produces the command prefix used to link an executable or shared library.
    string getLinkCommand() const;

    /// Produces the command prefix used to create a static archive.
    string getArchiveCommand() const;

    /// Applies the debug/release/profile preset represented by `configType`.
    void setConfigType(ConfigType configType);

    /// Returns whether a supported typed property is currently selected.
    template <typename T> bool evaluate(T property) const;

    /** Applies typed properties from left to right and returns `*this` for chaining. */
    template <typename T, typename... Property>
    LinkerFeatures &assign(T property, Property... properties) {
        if constexpr (std::is_same_v<T, AddressSanitizer>) addressSanitizer = property;
        else if constexpr (std::is_same_v<T, LeakSanitizer>) leakSanitizer = property;
        else if constexpr (std::is_same_v<T, ThreadSanitizer>) threadSanitizer = property;
        else if constexpr (std::is_same_v<T, UndefinedSanitizer>) undefinedSanitizer = property;
        else if constexpr (std::is_same_v<T, Coverage>) coverage = property;
        else if constexpr (std::is_same_v<T, LTO>) lto = property;
        else if constexpr (std::is_same_v<T, LTOMode>) ltoMode = property;
        else if constexpr (std::is_same_v<T, RuntimeLink>) runtimeLink = property;
        else if constexpr (std::is_same_v<T, RuntimeDebugging>) runtimeDebugging = property;
        else if constexpr (std::is_same_v<T, TargetOS>) targetOs = property;
        else if constexpr (std::is_same_v<T, DebugSymbols>) debugSymbols = property;
        else if constexpr (std::is_same_v<T, Profiling>) profiling = property;
        else if constexpr (std::is_same_v<T, Visibility>) visibility = property;
        else if constexpr (std::is_same_v<T, ConfigType>) setConfigType(property);
        else if constexpr (std::is_same_v<T, Arch>) arch = property;
        else if constexpr (std::is_same_v<T, AddressModel>) addModel = property;
        else if constexpr (std::is_same_v<T, DebugStore>) debugStore = property;
        else if constexpr (std::is_same_v<T, UserInterface>) userInterface = property;
        else if constexpr (std::is_same_v<T, InstructionSet>) instructionSet = property;
        else if constexpr (std::is_same_v<T, CpuType>) cpuType = property;
        else if constexpr (std::is_same_v<T, Strip>) strip = property;
        else if constexpr (std::is_same_v<T, CxxSTD>) cxxStd = property;
        else if constexpr (std::is_same_v<T, CxxSTDDialect>) cxxStdDialect = property;
        else if constexpr (std::is_same_v<T, Linker>) linker = property;
        else if constexpr (std::is_same_v<T, Archiver>) archiver = property;
        else if constexpr (std::is_same_v<T, Threading>) threading = property;
        else if constexpr (std::is_same_v<T, TargetType>) libraryType = property;

        if constexpr (sizeof...(properties)) {
            return assign(properties...);
        } else {
            return *this;
        }
    }

    /// Applies the entire property pack only when `assignBool` is true.
    template <typename T, typename... Condition>
    LinkerFeatures &assign(bool assignBool, T property, Condition... conditions)
    {
        if (assignBool) {
            return assign(property, conditions...);
        }
        return *this;
    }
};

template <typename T> bool LinkerFeatures::evaluate(T property) const
{
    if constexpr (std::is_same_v<decltype(property), Linker>)
    {
        return linker == property;
    }
    else if constexpr (std::is_same_v<decltype(property), BTFamily>)
    {
        return linker.bTFamily == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetOS>)
    {
        return targetOs == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Threading>)
    {
        return threading == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTD>)
    {
        return cxxStd == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTDDialect>)
    {
        return cxxStdDialect == property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugSymbols>)
    {
        return debugSymbols == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Profiling>)
    {
        return profiling == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Visibility>)
    {
        return visibility == property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressSanitizer>)
    {
        return addressSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), LeakSanitizer>)
    {
        return leakSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), ThreadSanitizer>)
    {
        return threadSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), UndefinedSanitizer>)
    {
        return undefinedSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Coverage>)
    {
        return coverage == property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTO>)
    {
        return lto == property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTOMode>)
    {
        return ltoMode == property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeLink>)
    {
        return runtimeLink == property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeDebugging>)
    {
        return runtimeDebugging == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Arch>)
    {
        return arch == property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressModel>)
    {
        return addModel == property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugStore>)
    {
        return debugStore == property;
    }
    else if constexpr (std::is_same_v<decltype(property), UserInterface>)
    {
        return userInterface == property;
    }
    else if constexpr (std::is_same_v<decltype(property), InstructionSet>)
    {
        return instructionSet == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CpuType>)
    {
        return cpuType == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Strip>)
    {
        return strip == property;
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        return property;
    }
    else
    {
        static_assert(false && "No property matched in LinkerFeatures::evaluate\n");
    }
}



/**
 * @brief Typed settings used to select a compiler and construct C/C++ compile commands.
 *
 * `initialize()` fills host/tool-dependent values that were left unspecified. In
 * particular, `ConfigType::NONE` becomes the release preset. Prefer assigning an explicit
 * `ConfigType` in multi-configuration projects so output intent is visible in `hmake.cpp`.
 */
struct CppCompilerFeatures
{
    AddressSanitizer addressSanitizer = AddressSanitizer::OFF;
    LeakSanitizer leakSanitizer = LeakSanitizer::OFF;
    ThreadSanitizer threadSanitizer = ThreadSanitizer::OFF;
    UndefinedSanitizer undefinedSanitizer = UndefinedSanitizer::OFF;

    Coverage coverage = Coverage::OFF;
    LTO lto = LTO::OFF;
    LTOMode ltoMode = LTOMode::FULL;
    RuntimeLink runtimeLink = RuntimeLink::SHARED;
    RuntimeDebugging runtimeDebugging = RuntimeDebugging::ON;
    TargetOS targetOs = TargetOS::NONE;
    DebugSymbols debugSymbols = DebugSymbols::ON;
    Profiling profiling = Profiling::OFF;
    Visibility localVisibility = Visibility::HIDDEN;

    ConfigType configType = ConfigType::NONE;

    // Following two are initialized in constructor
    // AddressModel and Architecture to target for.
    Arch arch = Arch::NONE;
    AddressModel addModel = AddressModel::NONE;

    // Windows Specifc
    DebugStore debugStore = DebugStore::OBJECT;

    StdLib stdLib = StdLib::NATIVE;

    Optimization optimization = Optimization::OFF;
    Inlining inlining = Inlining::OFF;
    Vectorize vectorize = Vectorize::OFF;
    Warnings warnings = Warnings::ALL;
    WarningsAsErrors warningsAsErrors = WarningsAsErrors::OFF;
    ExceptionHandling exceptionHandling = ExceptionHandling::ON;
    AsyncExceptions asyncExceptions = AsyncExceptions::OFF;
    ExternCNoThrow externCNoThrow = ExternCNoThrow::ON;
    RTTI rtti = RTTI::ON;

    // Used only for GCC
    TemplateDepth templateDepth{1024};

    // Following two are initialized in constructor
    // AddressModel and Architecture to target for.
    InstructionSet instructionSet = InstructionSet::OFF;
    CpuType cpuType = CpuType::NONE;

    CSourceTargetEnum cSourceTarget = CSourceTargetEnum::NO;

    CxxSTD cxxStd = CxxSTD::V_LATEST;
    CxxSTDDialect cxxStdDialect = CxxSTDDialect::ISO;
    Compiler compiler;

    // In threading-feature.jam the default value is single, but author here prefers multi
    Threading threading = Threading::MULTI;

    /// Resolves host defaults and the selected compiler. Called by `getCompileCommand()`.
    void initialize();

    void setCpuType();
    bool isCpuTypeG7();

    /// Applies a configuration preset and records it in `configType`.
    void setConfigType(ConfigType configType_);

    /// Produces flags for the selected compiler and current feature values.
    string getCompilerFlags() const;

    /// Resolves defaults and returns the compiler executable plus its flags.
    string getCompileCommand();

    /// Returns whether a supported typed property is currently selected.
    template <typename T> bool evaluate(T property) const;

    /** Applies typed properties from left to right and returns `*this` for chaining. */
    template <typename T, typename... Property>
    CppCompilerFeatures &assign(T property, Property... properties) {
        if constexpr (std::is_same_v<T, AddressSanitizer>) addressSanitizer = property;
        else if constexpr (std::is_same_v<T, LeakSanitizer>) leakSanitizer = property;
        else if constexpr (std::is_same_v<T, ThreadSanitizer>) threadSanitizer = property;
        else if constexpr (std::is_same_v<T, UndefinedSanitizer>) undefinedSanitizer = property;
        else if constexpr (std::is_same_v<T, Coverage>) coverage = property;
        else if constexpr (std::is_same_v<T, LTO>) lto = property;
        else if constexpr (std::is_same_v<T, LTOMode>) ltoMode = property;
        else if constexpr (std::is_same_v<T, RuntimeLink>) runtimeLink = property;
        else if constexpr (std::is_same_v<T, RuntimeDebugging>) runtimeDebugging = property;
        else if constexpr (std::is_same_v<T, TargetOS>) targetOs = property;
        else if constexpr (std::is_same_v<T, DebugSymbols>) debugSymbols = property;
        else if constexpr (std::is_same_v<T, Profiling>) profiling = property;
        else if constexpr (std::is_same_v<T, Visibility>) localVisibility = property;
        else if constexpr (std::is_same_v<T, ConfigType>) setConfigType(property);
        else if constexpr (std::is_same_v<T, Arch>) arch = property;
        else if constexpr (std::is_same_v<T, AddressModel>) addModel = property;
        else if constexpr (std::is_same_v<T, DebugStore>) debugStore = property;

        else if constexpr (std::is_same_v<T, StdLib>) stdLib = property;
        else if constexpr (std::is_same_v<T, Optimization>) optimization = property;
        else if constexpr (std::is_same_v<T, Inlining>) inlining = property;
        else if constexpr (std::is_same_v<T, Vectorize>) vectorize = property;
        else if constexpr (std::is_same_v<T, Warnings>) warnings = property;
        else if constexpr (std::is_same_v<T, WarningsAsErrors>) warningsAsErrors = property;
        else if constexpr (std::is_same_v<T, ExceptionHandling>) exceptionHandling = property;
        else if constexpr (std::is_same_v<T, AsyncExceptions>) asyncExceptions = property;
        else if constexpr (std::is_same_v<T, ExternCNoThrow>) externCNoThrow = property;
        else if constexpr (std::is_same_v<T, RTTI>) rtti = property;
        else if constexpr (std::is_same_v<T, InstructionSet>) instructionSet = property;
        else if constexpr (std::is_same_v<T, CpuType>) cpuType = property;
        else if constexpr (std::is_same_v<T, CSourceTargetEnum>) cSourceTarget = property;
        else if constexpr (std::is_same_v<T, CxxSTD>) cxxStd = property;
        else if constexpr (std::is_same_v<T, CxxSTDDialect>) cxxStdDialect = property;
        else if constexpr (std::is_same_v<T, Compiler>) compiler = property;
        else if constexpr (std::is_same_v<T, Threading>) threading = property;

        if constexpr (sizeof...(properties)) {
            return assign(properties...);
        } else {
            return *this;
        }
    }

    /// Applies the entire property pack only when `assignBool` is true.
    template <typename T, typename... Condition>
    CppCompilerFeatures &assign(bool assignBool, T property, Condition... conditions)
    {
        if (assignBool) {
            return assign(property, conditions...);
        }
        return *this;
    }
};

/**
 * @brief Configuration-wide ISPC toolchain and code-generation settings.
 *
 * The target OS, architecture, optimization, and debug settings follow ordinary
 * `Configuration::assign()` properties. `IspcTarget` adds only the owning C++ target's
 * finalized include paths and definitions to the command produced here.
 */
struct IspcCompilerFeatures
{
    Node *compiler = nullptr;
    TargetOS targetOs = TargetOS::NONE;
    Arch arch = Arch::NONE;
    AddressModel addressModel = AddressModel::NONE;
    ConfigType configType = ConfigType::NONE;
    Optimization optimization = Optimization::OFF;
    DebugSymbols debugSymbols = DebugSymbols::ON;

    /// ISPC's target vocabulary is intentionally retained as data because it evolves independently of HMake.
    vector<string> targets;
    /// Configuration-wide inputs exported by integrations such as UBT.
    vector<Node *> includeDirectories;
    vector<string> compileDefinitions;

    /// Inherits unset platform/configuration values and supplies the architecture's default target set.
    void initialize(const CppCompilerFeatures &cppFeatures);
    void setConfigType(ConfigType configType_);
    /// Produces the compiler, target tuple, and configuration-wide compile environment.
    string getCompileCommand() const;
    /// Produces object-only optimization/debug arguments; header generation deliberately omits them.
    string getObjectFlags() const;
    string_view getObjectSuffix() const;
};

template <typename T> bool CppCompilerFeatures::evaluate(T property) const
{
    if constexpr (std::is_same_v<decltype(property), CSourceTargetEnum>)
    {
        return cSourceTarget == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Compiler>)
    {
        return compiler == property;
    }
    else if constexpr (std::is_same_v<decltype(property), BTFamily>)
    {
        return compiler.bTFamily == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Threading>)
    {
        return threading == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTD>)
    {
        return cxxStd == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTDDialect>)
    {
        return cxxStdDialect == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Optimization>)
    {
        return optimization == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Inlining>)
    {
        return inlining == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Warnings>)
    {
        return warnings == property;
    }
    else if constexpr (std::is_same_v<decltype(property), WarningsAsErrors>)
    {
        return warningsAsErrors == property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExceptionHandling>)
    {
        return exceptionHandling == property;
    }
    else if constexpr (std::is_same_v<decltype(property), AsyncExceptions>)
    {
        return asyncExceptions == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Vectorize>)
    {
        return vectorize == property;
    }
    else if constexpr (std::is_same_v<decltype(property), RTTI>)
    {
        return rtti == property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExternCNoThrow>)
    {
        return externCNoThrow == property;
    }
    else if constexpr (std::is_same_v<decltype(property), StdLib>)
    {
        return stdLib == property;
    }
    else if constexpr (std::is_same_v<decltype(property), InstructionSet>)
    {
        return instructionSet == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CpuType>)
    {
        return cpuType == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetOS>)
    {
        return targetOs == property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugSymbols>)
    {
        return debugSymbols == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Profiling>)
    {
        return profiling == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Visibility>)
    {
        return localVisibility == property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressSanitizer>)
    {
        return addressSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), LeakSanitizer>)
    {
        return leakSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), ThreadSanitizer>)
    {
        return threadSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), UndefinedSanitizer>)
    {
        return undefinedSanitizer == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Coverage>)
    {
        return coverage == property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTO>)
    {
        return lto == property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTOMode>)
    {
        return ltoMode == property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeLink>)
    {
        return runtimeLink == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Arch>)
    {
        return arch == property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressModel>)
    {
        return addModel == property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugStore>)
    {
        return debugStore == property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeDebugging>)
    {
        return runtimeDebugging == property;
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        return property;
    }
    else
    {
        static_assert(false && "No property matched in CppCompilerFeatures::evaluate\n");
    }
}

#endif // HMAKE_FEATURES_HPP
