#ifndef HMAKE_FEATURES_HPP
#define HMAKE_FEATURES_HPP
#ifdef USE_HEADER_UNITS
import "BuildTools.hpp";
import "Cache.hpp";
import "InclNodeTargetMap.hpp";
import "FeaturesConvenienceFunctions.hpp";
import "OS.hpp";
import "SpecialNodes.hpp";
import "TargetType.hpp";
import <vector>;
#else
#include "BuildTools.hpp"
#include "Cache.hpp"
#include "FeaturesConvenienceFunctions.hpp"
#include "OS.hpp"
#include "SpecialNodes.hpp"
#include "TargetType.hpp"
#include <vector>
#endif

using std::vector;

enum class TranslateInclude : bool
{
    NO,
    YES,
};

enum class TreatModuleAsSource : bool
{
    NO,
    YES
};

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

// TODO
// A feature for --nostdinc and --nostdinc++

// In b2 features every non-optional, non-free feature must have a value. Because hmake does not have optional features,
// all optional features have extra enum value OFF declared here. A feature default value is given by the first value
// listed in the feature declaration which is imitated in CompilerFeautres and LinkerFeatures.

enum class Arch : char // Architecture
{
    X86,
    IA64,
    SPARC,
    POWER,
    LOONGARCH,
    MIPS,
    MIPS1,
    MIPS2,
    MIPS3,
    MIPS4,
    MIPS32,
    MIPS32R2,
    MIPS64,
    PARISC,
    ARM,
    S390X,
    ARM_P_X86, // arm+x86
    NONE,
};
void to_json(Json &j, const Arch &arch);
void from_json(const Json &j, Arch &arch);

enum class AddressModel : char // AddressModel
{
    A_16,
    A_32,
    A_64,
    A_32_64,
    NONE,
};
void to_json(Json &j, const AddressModel &am);
void from_json(const Json &j, AddressModel &am);

struct CxxFlags : string
{
};

struct LinkFlags : string
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

void to_json(Json &j, const Define &cd);
void from_json(const Json &j, Define &cd);

enum class Threading : bool
{
    SINGLE,
    MULTI
};

enum class Warnings : char
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

enum class Visibility : char
{
    OFF,
    GLOBAL,
    HIDDEN,
    PROTECTED,
};

enum class ConfigType : char
{
    DEBUG,
    RELEASE,
    PROFILE,
    NONE,
};

enum class AddressSanitizer : char
{
    OFF,
    NORECOVER,
    ON,
};

enum class LeakSanitizer : char
{
    OFF,
    NORECOVER,
    ON,
};

enum class ThreadSanitizer : char
{
    OFF,
    NORECOVER,
    ON,
};

enum class UndefinedSanitizer : char
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

enum class LTOMode : char
{
    FAT,
    FULL,
    THIN,
};

enum class StdLib : char
{
    NATIVE,
    GNU,
    GNU11,
    LIBCPP, // libc++
    SUN_STLPORT,
    APACHE,
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

void to_json(Json &json, const OS &osLocal);
void from_json(const Json &json, OS &osLocal);

string getActualNameFromTargetName(TargetType targetType, enum OS osLocal, const string &targetName);
string getTargetNameFromActualName(TargetType targetType, enum OS osLocal, const string &actualName);
string getSlashedExecutableName(const string &name);

enum class CxxSTD : char
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

enum class CxxSTDDialect : char
{
    ISO,
    GNU,
    MS,
};

enum class TargetOS : char
{
    AIX,
    ANDROID,
    APPLETV,
    BSD,
    CYGWIN,
    DARWIN,
    FREEBSD,
    FREERTOS,
    HAIKU,
    HPUX,
    IPHONE,
    LINUX_,
    NETBSD,
    OPENBSD,
    OSF,
    QNX,
    QNXNTO,
    SGI,
    SOLARIS,
    UNIX_,
    UNIXWARE,
    VMS,
    VXWORKS,
    WINDOWS,
    NONE,
};

enum class Language : char
{
    C,
    CPP,
    OBJECTIVE_C,
    OBJECTIVE_CPP,
};

enum class Optimization : char
{
    OFF,
    SPEED,
    SPACE,
    MINIMAL,
    DEBUG,
};

enum class Inlining : char
{
    OFF,
    ON,
    FULL,
};

enum class Vectorize : char
{
    OFF,
    ON,
    FULL,
};

enum class UserInterface : char
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
    // x86 and x86-64
    OFF,
    native,
    i486,
    i586,
    i686,
    pentium,
    pentium_mmx,
    pentiumpro,
    pentium2,
    pentium3,
    pentium3m,
    pentium_m,
    pentium4,
    pentium4m,
    prescott,
    nocona,
    core2,
    corei7,
    corei7_avx,
    core_avx_i,
    conroe,
    conroe_xe,
    conroe_l,
    allendale,
    merom,
    merom_xe,
    kentsfield,
    kentsfield_xe,
    penryn,
    wolfdale,
    yorksfield,
    nehalem,
    sandy_bridge,
    ivy_bridge,
    haswell,
    broadwell,
    skylake,
    skylake_avx512,
    cannonlake,
    icelake_client,
    icelake_server,
    cascadelake,
    cooperlake,
    tigerlake,
    rocketlake,
    alderlake,
    sapphirerapids,
    atom,
    k6,
    k6_2,
    k6_3,
    athlon,
    athlon_tbird,
    athlon_4,
    athlon_xp,
    athlon_mp,
    k8,
    opteron,
    athlon64,
    athlon_fx,
    k8_sse3,
    opteron_sse3,
    athlon64_sse3,
    amdfam10,
    barcelona,
    bdver1,
    bdver2,
    bdver3,
    bdver4,
    btver1,
    btver2,
    znver1,
    znver2,
    znver3,
    winchip_c6,
    winchip2,
    c3,
    c3_2,
    c7,

    // ia64
    itanium,
    itanium1,
    merced,
    itanium2,
    mckinley,

    // Sparc
    v7,
    cypress,
    v8,
    supersparc,
    sparclite,
    hypersparc,
    sparclite86x,
    f930,
    f934,
    sparclet,
    tsc701,
    v9,
    ultrasparc,
    ultrasparc3,

    // RS/6000 & PowerPC
    V_401,
    V_403,
    V_405,
    V_405fp,
    V_440,
    V_440fp,
    V_505,
    V_601,
    V_602,
    V_603,
    V_603e,
    V_604,
    V_604e,
    V_620,
    V_630,
    V_740,
    V_7400,
    V_7450,
    V_750,
    V_801,
    V_821,
    V_823,
    V_860,
    V_970,
    V_8540,
    power_common,
    ec603e,
    g3,
    g4,
    g5,
    power,
    power2,
    power3,
    power4,
    power5,
    powerpc,
    powerpc64,
    rios,
    rios1,
    rsc,
    rios2,
    rs64a,

    // MIPS
    V_4kc,
    V_4km,
    V_4kp,
    V_4ksc,
    V_4kec,
    V_4kem,
    V_4kep,
    V_4ksd,
    V_5kc,
    V_5kf,
    V_20kc,
    V_24kc,
    V_24kf2_1,
    V_24kf1_1,
    V_24kec,
    V_24kef2_1,
    V_24kef1_1,
    V_34kc,
    V_34kf2_1,
    V_34kf1_1,
    V_34kn,
    V_74kc,
    V_74kf2_1,
    V_74kf1_1,
    V_74kf3_2,
    V_1004kc,
    V_1004kf2_1,
    V_1004kf1_1,
    i6400,
    i6500,
    interaptiv,
    loongson2e,
    loongson2f,
    loongson3a,
    gs464,
    gs464e,
    gs264e,
    m4k,
    m14k,
    m14kc,
    m14ke,
    m14kec,
    m5100,
    m5101,
    octeon,
    octeon_p,
    octeon2,
    octeon3,
    orion,
    p5600,
    p6600,
    r2000,
    r3000,
    r3900,
    r4000,
    r4400,
    r4600,
    r4650,
    r4700,
    r5900,
    r6000,
    r8000,
    rm7000,
    rm9000,
    r10000,
    r12000,
    r14000,
    r16000,
    sb1,
    sr71000,
    vr4100,
    vr4111,
    vr4120,
    vr4130,
    vr4300,
    vr5000,
    vr5400,
    vr5500,
    xlr,
    xlp,

    // HP/PA-RISC
    V_700,
    V_7100,
    V_7100lc,
    V_7200,
    V_7300,
    V_8000,

    // Advanced RISC Machines
    armv2,
    armv2a,
    armv3,
    armv3m,
    armv4,
    armv4t,
    armv5,
    armv5t,
    armv5te,
    armv6,
    armv6j,
    iwmmxt,
    ep9312,
    armv7,
    armv7s,

    cortex_a9_p_vfpv3,
    cortex_a53,
    cortex_r5,
    cortex_r5_p_vfpv3_d16,

    // z Systems (aka s390x)
    z196,
    zEC12,
    z13,
    z14,
    z15,
};

// Declared on Line 2143 in msvc.jam
enum class CpuType : char
{
    G5,
    G6,
    EM64T,
    AMD64,
    G7,
    ITANIUM,
    ITANIUM2,
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

struct LinkerFlags
{
    // GCC
    string OPTIONS;
    string OPTIONS_LINK;
    string LANG;
    string RPATH_OPTION_LINK;
    string FINDLIBS_ST_PFX_LINK;
    string FINDLIBS_SA_PFX_LINK;
    string HAVE_SONAME_LINK;
    string SONAME_OPTION_LINK;
    string DOT_IMPLIB_COMMAND_LINK_DLL;

    // Following two are directly used instead of being set
    string RPATH_LINK;
    string RPATH_LINK_LINK;

    bool isRpathOs = false;
    // MSVC
    string FINDLIBS_SA_LINK;
    string DOT_LD_LINK;
    string DOT_LD_ARCHIVE;
    string LINKFLAGS_LINK;
    string PDB_CFLAG;
    string ASMFLAGS_ASM;
    string PDB_LINKFLAG;
    string LINKFLAGS_MSVC;
};

struct LinkerFeatures : FeatureConvenienceFunctions<LinkerFeatures>
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
    LinkerFlags getLinkerFlags();
    void setConfigType(ConfigType configType);
    template <typename T> bool evaluate(T property) const;
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

// Separate this in GccCompilerFlags and MSVCCompilerFlags
struct CompilerFlags
{
    // GCC
    string OPTIONS;
    string OPTIONS_COMPILE_CPP;
    string OPTIONS_COMPILE;
    string DEFINES_COMPILE_CPP;
    string LANG;

    string TRANSLATE_INCLUDE;
    // MSVC
    string DOT_CC_COMPILE;
    string DOT_ASM_COMPILE;
    string DOT_ASM_OUTPUT_COMPILE;
    string DOT_LD_ARCHIVE;
    string PCH_FILE_COMPILE;
    string PCH_SOURCE_COMPILE;
    string PCH_HEADER_COMPILE;
    string PDB_CFLAG;
    string ASMFLAGS_ASM;
    string CPP_FLAGS_COMPILE_CPP;
    string CPP_FLAGS_COMPILE;
};

struct CppCompilerFeatures : FeatureConvenienceFunctions<CppCompilerFeatures>
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
    ExternCNoThrow externCNoThrow = ExternCNoThrow::OFF;
    RTTI rtti = RTTI::ON;
    TranslateInclude translateInclude = TranslateInclude::NO;
    TreatModuleAsSource treatModuleAsSource = TreatModuleAsSource::NO;

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
    ScannerTool scanner;

    // In threading-feature.jam the default value is single, but author here prefers multi
    Threading threading = Threading::MULTI;

    void initialize();

    void setCpuType();
    bool isCpuTypeG7();
    void setConfigType(ConfigType configType_);
    CompilerFlags getCompilerFlags() const;
    template <typename T> bool evaluate(T property) const;
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
    else if constexpr (std::is_same_v<decltype(property), TranslateInclude>)
    {
        return translateInclude == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TreatModuleAsSource>)
    {
        return treatModuleAsSource == property;
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
