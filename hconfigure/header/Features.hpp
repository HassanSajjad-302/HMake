#ifndef HMAKE_FEATURES_HPP
#define HMAKE_FEATURES_HPP
#include "BuildTools.hpp"
#include "Environment.hpp"
#include "SMFile.hpp"
#include "TargetType.hpp"
#include <vector>

using std::vector;

enum class Arch // Architecture
{
    X86,
    IA64,
    SPARC,
    POWER,
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
    LOONGARCH,
};

enum class AM // AddressModel
{
    A_32,
    A_64,
};

struct CxxFlags : string
{
};

struct LinkFlags : string
{
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

enum class Link
{
    STATIC,
    SHARED,
};

enum class Threading
{
    SINGLE,
    MULTI
};

enum class Warnings
{
    ON,
    ALL,
    EXTRA,
    PEDANTIC,
    OFF,
};

enum class WarningsAsErrors
{
    OFF,
    ON,
};

enum class ExceptionHandling
{
    OFF,
    ON,
};

enum class AsyncExceptions
{
    OFF,
    ON,
};

enum class ExternCNoThrow
{
    OFF,
    ON,
};

enum class RTTI
{
    OFF,
    ON,
};

enum class DebugSymbols
{
    OFF,
    ON,
};

enum class Profiling
{
    OFF,
    ON,
};

enum class LocalVisibility
{
    GLOBAL,
    HIDDEN,
    PROTECTED,
};

enum class AddressSanitizer
{
    NORECOVER,
    ON,
};

enum class LeakSanitizer
{
    NORECOVER,
    ON,
};

enum class ThreadSanitizer
{
    NORECOVER,
    ON,
};

enum class UndefinedSanitizer
{
    NORECOVER,
    ON,
};

enum class LTO
{
    OFF,
    ON,
};

enum class LTOMode
{
    FAT,
    FULL,
    THIN,
};

enum class StdLib
{
    NATIVE,
    GNU,
    GNU11,
    LIBCPP, // libc++
    SUN_STLPORT,
    APACHE,
};

enum class Coverage
{
    OFF,
    ON,
};

enum class RuntimeLink
{
    SHARED,
    STATIC,
};

enum class RuntimeDebugging
{
    OFF,
    ON
};

enum class OS
{
    AIX,
    CYGWIN,
    LINUX,
    MACOSX,
    NT,
    VMS,
};
void to_json(Json &json, const OS &os);
void from_json(const Json &json, OS &os);

string getActualNameFromTargetName(TargetType targetType, const enum OS &os, const string &targetName);
string getTargetNameFromActualName(TargetType targetType, const enum OS &os, const string &actualName);

enum class CxxSTD
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

enum class CxxSTDDialect
{
    ISO,
    GNU,
    MS,
};

enum class TargetOS
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
};

enum class Language
{
    C,
    CPP,
    OBJECTIVE_C,
    OBJECTIVE_CPP,
};

enum class TS
{
    GCC,
    MSVC,
    CLANG,
    DARWIN,
    PGI,
    SUN,
    GCC_3_4_4,
    GCC_4,
    GCC_4_3_4,
    GCC_4_4_0,
    GCC_4_5_0,
    GCC_4_6_0,
    GCC_4_6_3,
    GCC_4_7_0,
    GCC_4_8_0,
    GCC_5,
    DARWIN_4,
    DARWIN_5,
    DARWIN_4_6_2,
    DARWIN_4_7_0,
    CLANG_3_0,
    PATHSCALE,
    INTEL,
    VACPP,
};
struct ToolSet
{
    string name;
    Version version;
    TS ts;

  public:
    explicit ToolSet(TS ts_ = TS::GCC);
};

enum class AddressModel
{
    A_32,
    A_64,
};

enum class ConfigType
{
    DEBUG,
    RELEASE
};
void to_json(Json &json, const ConfigType &configType);
void from_json(const Json &json, ConfigType &configType);

enum class Optimization
{
    OFF,
    SPEED,
    SPACE,
    MINIMAL,
    DEBUG,
};

enum class Inlining
{
    OFF,
    ON,
    FULL,
};

enum class Vectorize
{
    OFF,
    ON,
    FULL,
};

struct Features
{
    Environment environment;
    // Sanitizers
    AddressSanitizer addressSanitizer{};
    LeakSanitizer leakSanitizer{};
    ThreadSanitizer threadSanitizer;
    UndefinedSanitizer undefinedSanitizer;
    Coverage coverage;

    // LTO
    LTO lto;
    LTOMode ltoMode;

    // stdlib
    StdLib stdLib;

    RuntimeLink runtimeLink;
    RuntimeDebugging runtimeDebugging;

    Optimization optimization;
    Inlining inlining;
    Vectorize vectorize;
    Warnings warnings;
    WarningsAsErrors warningsAsErrors;
    ExceptionHandling exceptionHandling;
    AsyncExceptions asyncExceptions;
    RTTI rtti;
    ExternCNoThrow externCNoThrow;
    DebugSymbols debugSymbols;
    Profiling profiling;
    LocalVisibility localVisibility;
    ToolSet toolSet;
    TargetOS targetOs;
    AddressModel addModel;
    ConfigType configurationType;
    CxxSTD cxxStd;
    CxxSTDDialect cxxStdDialect;
    Compiler compiler;
    Linker linker;
    Archiver archiver;
    Threading threading;
    Link link;
    vector<const Node *> privateIncludes;
    vector<const Node *> privateHUIncludes;
    string privateCompilerFlags;
    string privateLinkerFlags;
    vector<Define> privateCompileDefinitions;
    TargetType libraryType;
    void initializeFromCacheFunc();
};

#endif // HMAKE_FEATURES_HPP
