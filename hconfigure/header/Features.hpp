#ifndef HMAKE_FEATURES_HPP
#define HMAKE_FEATURES_HPP

#include "BuildTools.hpp"
#include "Cache.hpp"

#include "TargetType.hpp"
#include <vector>

using std::vector;

enum class CSourceTargetEnum
{
    NO,
    YES
};

enum class CopyDLLToExeDirOnNTOs : bool
{
    NO,
    YES
};

enum class DefineDLLPrivate : bool
{
    YES,
    NO,
};

enum class DefineDLLInterface : bool
{
    YES,
    NO,
};

// In b2 features every non-optional, non-free feature must have a value. Because hmake does not have optional features,
// all optional features have extra enum value OFF declared here. A feature default value is given by the first value
// listed in the feature declaration which is imitated in CompilerFeautres and LinkerFeatures.

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


struct CxxFlags : string
{
};

struct TemplateDepth
{
    unsigned long long templateDepth;
    explicit TemplateDepth(unsigned long long templateDepth_);
};

struct Define
{
    string name;
    string value;
    Define() = default;
    explicit Define(string name_, string value_ = "");
};



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

string getActualNameFromTargetName(TargetType targetType, enum OS osLocal, const string &targetName);
string getTargetNameFromActualName(TargetType targetType, enum OS osLocal, const string &actualName);
string getSlashedExecutableName(const string &name);

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

struct DSCPrebuiltFeatures
{
    DefineDLLInterface defineDllInterface = DefineDLLInterface::NO;
};

struct DSCFeatures : DSCPrebuiltFeatures
{
    DefineDLLPrivate defineDllPrivate = DefineDLLPrivate::NO;
};

struct PrebuiltLinkerFeatures
{
    CopyDLLToExeDirOnNTOs copyToExeDirOnNtOs = CopyDLLToExeDirOnNTOs::YES;
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
    string getLinkerFlags();
    string getLinkCommand() const;
    string getArchiveCommand() const;
    void setConfigType(ConfigType configType);
    template <typename T> bool evaluate(T property) const;

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

    void initialize();

    void setCpuType();
    bool isCpuTypeG7();
    void setConfigType(ConfigType configType_);
    string getCompilerFlags() const;
    string getCompileCommand();
    template <typename T> bool evaluate(T property) const;

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

    template <typename T, typename... Condition>
    CppCompilerFeatures &assign(bool assignBool, T property, Condition... conditions)
    {
        if (assignBool) {
            return assign(property, conditions...);
        }
        return *this;
    }
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
