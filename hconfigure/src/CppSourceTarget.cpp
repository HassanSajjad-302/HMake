#include "CppSourceTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Cache.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <fstream>
#include <regex>
#include <utility>

using std::filesystem::create_directory, std::filesystem::file_time_type, std::filesystem::recursive_directory_iterator,
    std::ifstream, std::ofstream, std::regex, std::regex_error;

SourceDirectory::SourceDirectory(string sourceDirectory_, string regex_)
    : sourceDirectory{Node::getNodeFromString(sourceDirectory_, false)}, regex{std::move(regex_)}
{
}

void SourceDirectory::populateSourceOrSMFiles(class CppSourceTarget &target, bool sourceFiles)
{
}

void to_json(Json &j, const SourceDirectory &sourceDirectory)
{
    j[JConsts::path] = sourceDirectory.sourceDirectory;
    j[JConsts::regexString] = sourceDirectory.regex;
}

bool operator<(const SourceDirectory &lhs, const SourceDirectory &rhs)
{
    return std::tie(lhs.sourceDirectory, lhs.regex) < std::tie(rhs.sourceDirectory, rhs.regex);
}

void CppSourceTarget::addNodeInSourceFileDependencies(const string &str)
{
    auto [pos, ok] = sourceFileDependencies.emplace(this, str, ResultType::SOURCENODE);
    auto &sourceNode = const_cast<SourceNode &>(*pos);
    sourceNode.presentInSource = true;
}

void CppSourceTarget::addNodeInModuleSourceFileDependencies(const std::string &str, bool angle)
{
    auto [pos, ok] = moduleSourceFileDependencies.emplace(this, str);
    auto &smFile = const_cast<SMFile &>(*pos);
    smFile.presentInSource = true;
}

void CppSourceTarget::setCpuType()
{
    // Based on msvc.jam Line 2141
    if (OR(InstructionSet::i586, InstructionSet::pentium, InstructionSet::pentium_mmx))
    {
        cpuType = CpuType::G5;
    }
    else if (OR(InstructionSet::i686, InstructionSet::pentiumpro, InstructionSet::pentium2, InstructionSet::pentium3,
                InstructionSet::pentium3m, InstructionSet::pentium_m, InstructionSet::k6, InstructionSet::k6_2,
                InstructionSet::k6_3, InstructionSet::winchip_c6, InstructionSet::winchip2, InstructionSet::c3,
                InstructionSet::c3_2, InstructionSet::c7))
    {
        cpuType = CpuType::G6;
    }
    else if (OR(InstructionSet::prescott, InstructionSet::nocona, InstructionSet::core2, InstructionSet::corei7,
                InstructionSet::corei7_avx, InstructionSet::core_avx_i, InstructionSet::conroe,
                InstructionSet::conroe_xe, InstructionSet::conroe_l, InstructionSet::allendale, InstructionSet::merom,
                InstructionSet::merom_xe, InstructionSet::kentsfield, InstructionSet::kentsfield_xe,
                InstructionSet::penryn, InstructionSet::wolfdale, InstructionSet::yorksfield, InstructionSet::nehalem,
                InstructionSet::sandy_bridge, InstructionSet::ivy_bridge, InstructionSet::haswell,
                InstructionSet::broadwell, InstructionSet::skylake, InstructionSet::skylake_avx512,
                InstructionSet::cannonlake, InstructionSet::icelake_client, InstructionSet::icelake_server,
                InstructionSet::cascadelake, InstructionSet::cooperlake, InstructionSet::tigerlake,
                InstructionSet::rocketlake, InstructionSet::alderlake, InstructionSet::sapphirerapids))
    {
        cpuType = CpuType::EM64T;
    }
    else if (OR(InstructionSet::k8, InstructionSet::opteron, InstructionSet::athlon64, InstructionSet::athlon_fx,
                InstructionSet::k8_sse3, InstructionSet::opteron_sse3, InstructionSet::athlon64_sse3,
                InstructionSet::amdfam10, InstructionSet::barcelona, InstructionSet::bdver1, InstructionSet::bdver2,
                InstructionSet::bdver3, InstructionSet::bdver4, InstructionSet::btver1, InstructionSet::btver2,
                InstructionSet::znver1, InstructionSet::znver2, InstructionSet::znver3))
    {
        cpuType = CpuType::AMD64;
    }
    else if (OR(InstructionSet::itanium, InstructionSet::itanium2, InstructionSet::merced))
    {
        cpuType = CpuType::ITANIUM;
    }
    else if (OR(InstructionSet::itanium2, InstructionSet::mckinley))
    {
        cpuType = CpuType::ITANIUM2;
    }
    else if (OR(InstructionSet::armv2, InstructionSet::armv2a, InstructionSet::armv3, InstructionSet::armv3m,
                InstructionSet::armv4, InstructionSet::armv4t, InstructionSet::armv5, InstructionSet::armv5t,
                InstructionSet::armv5te, InstructionSet::armv6, InstructionSet::armv6j, InstructionSet::iwmmxt,
                InstructionSet::ep9312, InstructionSet::armv7, InstructionSet::armv7s))
    {
        cpuType = CpuType::ARM;
    }
    // cpuType G7 is not being assigned. Line 2158. Function isTypeG7 will return true if the values are equivalent.
}

bool CppSourceTarget::isCpuTypeG7()
{
    return OR(InstructionSet::pentium4, InstructionSet::pentium4m, InstructionSet::athlon, InstructionSet::athlon_tbird,
              InstructionSet::athlon_4, InstructionSet::athlon_xp, InstructionSet::athlon_mp, CpuType::EM64T,
              CpuType::AMD64);
}

CppSourceTarget::CppSourceTarget(string name_, const bool initializeFromCache)
    : CTarget{std::move(name_)}, BTarget(ResultType::LINK)
{
    if (initializeFromCache)
    {
        initializeFromCacheFunc();
    }
}

CppSourceTarget::CppSourceTarget(string name_, LinkOrArchiveTarget &linkOrArchiveTarget, const bool initializeFromCache)
    : CTarget{std::move(name_), linkOrArchiveTarget, false}, BTarget(ResultType::LINK),
      linkOrArchiveTarget(&linkOrArchiveTarget)
{
    if (initializeFromCache)
    {
        initializeFromCacheFunc();
    }
}

CppSourceTarget::CppSourceTarget(string name_, Variant &variant)
    : CTarget{std::move(name_), variant}, BTarget(ResultType::LINK)
{
    static_cast<Features &>(*this) = static_cast<const Features &>(variant);
}

void CppSourceTarget::updateBTarget()
{
    pruneAndSaveBuildCache(true);
}

void CppSourceTarget::printMutexLockRoutine()
{
    // TODO
    //  This function not needed.
}

void CppSourceTarget::checkForHeaderUnitsCache()
{
    // TODO:
    //  Currently using targetSet instead of variantFilePaths. With
    //  scoped-modules feature, it should use module scope, be it variantFilePaths
    //  or any module-scope

    set<const string *> addressed;
    // TODO:
    // Should be iterating over headerUnits scope instead.
    const auto [pos, Ok] = addressed.emplace(variantFilePath);
    if (!Ok)
    {
        return;
    }
    // SHU system header units     AHU application header units
    path shuCachePath = path(**pos).parent_path() / "shus/headerUnits.cache";
    path ahuCachePath = path(**pos).parent_path() / "ahus/headerUnits.cache";
    if (exists(shuCachePath))
    {
        Json shuCacheJson;
        ifstream(shuCachePath) >> shuCacheJson;
        for (const Json &shu : shuCacheJson)
        {
            moduleSourceFileDependencies.emplace(this, shu);
        }
    }
    else
    {
        create_directory(shuCachePath.parent_path());
    }
    if (exists(ahuCachePath))
    {
        Json ahuCacheJson;
        ifstream(shuCachePath) >> ahuCacheJson;
        for (auto &ahu : ahuCacheJson)
        {
            const auto &[pos1, Ok1] = moduleSourceFileDependencies.emplace(this, ahu);
            const_cast<SMFile &>(*pos1).presentInCache = true;
        }
    }
    else
    {
        create_directory(ahuCachePath.parent_path());
    }
}

void CppSourceTarget::createHeaderUnits()
{
    // TODO:
    //  Currently using targetSet instead of variantFilePaths. With
    //  scoped-modules feature, it should use module scope, be it variantFilePaths
    //  or any module-scope

    set<const string *> addressed;
    const auto [pos, Ok] = addressed.emplace(variantFilePath);
    if (!Ok)
    {
        return;
    }
    // SHU system header units     AHU application header units
    path shuCachePath = path(**pos) / "shus/headerUnits.cache";
    path ahuCachePath = path(**pos) / "ahus/headerUnits.cache";
    if (exists(shuCachePath))
    {
        Json shuCacheJson;
        ifstream(shuCachePath) >> shuCacheJson;
        for (auto &shu : shuCacheJson)
        {
            moduleSourceFileDependencies.emplace(this, shu);
        }
    }
    if (exists(ahuCachePath))
    {
        Json ahuCacheJson;
        ifstream(shuCachePath) >> ahuCacheJson;
        for (auto &ahu : ahuCacheJson)
        {
            const auto &[pos1, Ok1] = moduleSourceFileDependencies.emplace(this, ahu);
            const_cast<SMFile &>(*pos1).presentInCache = true;
        }
    }
}

void CppSourceTarget::setPropertiesFlagsMSVC()
{
    // msvc.jam supports multiple tools such as assembler, compiler, mc-compiler(message-catalogue-compiler),
    // idl-compiler(interface-definition-compiler) and manifest-tool. HMake does not support these and  only supports
    // link.exe, lib.exe and cl.exe. While the msvc.jam also supports older VS and store and phone Windows API, HMake
    // only supports the recent Visual Studio version and the Desktop API. Besides these limitation, msvc.jam is tried
    // to be imitated here.

    // Hassan Sajjad
    // I don't have complete confidence about correctness of following info.

    // On Line 2220, auto-detect-toolset-versions calls register-configuration. This call version.set with the version
    // and compiler path(obtained from default-path rule). After that, register toolset registers all generators. And
    // once msvc toolset is init, configure-relly is called that sets the setup script of previously set
    // .version/configuration. setup-script captures all of environment-variables set when vcvarsall.bat for a
    // configuration is run in file "msvc-setup.bat" file. This batch file run before the generator actions. This is
    // .SETUP variable in actions.

    // all variables in actions are in CAPITALS

    // Line 1560
    string defaultAssembler = EVALUATE(Arch::IA64) ? "ias" : "";
    if (EVALUATE(Arch::X86))
    {
        defaultAssembler += GET_FLAG_EVALUATE(AddressModel::A_64, "ml64", AddressModel::A_32, "ml -coff");
    }
    else if (EVALUATE(Arch::ARM))
    {
        defaultAssembler += GET_FLAG_EVALUATE(AddressModel::A_64, "armasm64", AddressModel::A_32, "armasm");
    }
    string assemblerFlags = GET_FLAG_EVALUATE(OR(Arch::X86, Arch::IA64), "-c -Zp4 -Cp -Cx");
    string assemblerOutputFlag = GET_FLAG_EVALUATE(OR(Arch::X86, Arch::IA64), "-Fo", Arch::ARM, "-o");
    // Line 1618

    // Line 1650
    string DOT_CC_COMPILE = "/Zm800 -nologo";
    string DOT_ASM_COMPILE = defaultAssembler + assemblerFlags + "-nologo";
    string DOT_ASM_OUTPUT_COMPILE = assemblerOutputFlag;
    string DOT_LD_LINK = "/NOLOGO /INCREMENTAL:NO";
    string DOT_LD_ARCHIVE = "lib /NOLOGO";

    // Line 1670
    string OPTIONS_COMPILE = GET_FLAG_EVALUATE(LTO::ON, "/GL");
    string LINKFLAGS_LINK = GET_FLAG_EVALUATE(LTO::ON, "/LTCG");
    // End-Line 1682

    // Function completed. Jumping to rule configure-version-specific.
    // Line 444
    // Only flags effecting latest MSVC tools (14.3) are supported.

    OPTIONS_COMPILE += "/Zc:forScope /Zc:wchar_t";
    string CPP_FLAGS_COMPILE_CPP = "/wd4675";
    OPTIONS_COMPILE += GET_FLAG_EVALUATE(Warnings::OFF, "/wd4996");
    OPTIONS_COMPILE += "/Zc:inline";
    OPTIONS_COMPILE += GET_FLAG_EVALUATE(OR(Optimization::SPEED, Optimization::SPACE), "/Gw");
    CPP_FLAGS_COMPILE_CPP += "/Zc:throwingNew";

    // Line 492
    OPTIONS_COMPILE += GET_FLAG_EVALUATE(AddressSanitizer::ON, "/fsanitize=address /FS");
    LINKFLAGS_LINK += GET_FLAG_EVALUATE(AddressSanitizer::ON, "-incremental\\:no");

    if (EVALUATE(AddressModel::A_64))
    {
        // The various 64 bit runtime asan support libraries and related flags.
        string FINDLIBS_SA_LINK =
            GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::SHARED),
                              "clang_rt.asan_dynamic-x86_64 clang_rt.asan_dynamic_runtime_thunk-x86_64");
        LINKFLAGS_LINK += GET_FLAG_EVALUATE(
            AND(AddressSanitizer::ON, RuntimeLink::SHARED),
            R"(/wholearchive\:"clang_rt.asan_dynamic-x86_64.lib /wholearchive\:"clang_rt.asan_dynamic_runtime_thunk-x86_64.lib)");
        FINDLIBS_SA_LINK += GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC, TargetType::EXECUTABLE),
                                              "clang_rt.asan-x86_64 clang_rt.asan_cxx-x86_64 ");
        LINKFLAGS_LINK += GET_FLAG_EVALUATE(
            AND(AddressSanitizer::ON, RuntimeLink::STATIC, TargetType::EXECUTABLE),
            R"(/wholearchive\:"clang_rt.asan-x86_64.lib /wholearchive\:"clang_rt.asan_cxx-x86_64.lib")");
        string FINDLIBS_SA_LINK_DLL =
            GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC), "clang_rt.asan_dll_thunk-x86_64");
        string LINKFLAGS_LINK_DLL = GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC),
                                                      R"(/wholearchive\:"clang_rt.asan_dll_thunk-x86_64.lib")");
    }
    else if (EVALUATE(AddressModel::A_32))
    {
        // The various 32 bit runtime asan support libraries and related flags.

        string FINDLIBS_SA_LINK =
            GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::SHARED),
                              "clang_rt.asan_dynamic-i386 clang_rt.asan_dynamic_runtime_thunk-i386");
        LINKFLAGS_LINK += GET_FLAG_EVALUATE(
            AND(AddressSanitizer::ON, RuntimeLink::SHARED),
            R"(/wholearchive\:"clang_rt.asan_dynamic-i386.lib /wholearchive\:"clang_rt.asan_dynamic_runtime_thunk-i386.lib)");
        FINDLIBS_SA_LINK += GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC, TargetType::EXECUTABLE),
                                              "clang_rt.asan-i386 clang_rt.asan_cxx-i386 ");
        LINKFLAGS_LINK +=
            GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC, TargetType::EXECUTABLE),
                              R"(/wholearchive\:"clang_rt.asan-i386.lib /wholearchive\:"clang_rt.asan_cxx-i386.lib")");
        string FINDLIBS_SA_LINK_DLL =
            GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC), "clang_rt.asan_dll_thunk-i386");
        string LINKFLAGS_LINK_DLL = GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC),
                                                      R"(/wholearchive\:"clang_rt.asan_dll_thunk-i386.lib")");
    }

    // Line 586
    if (AND(Arch::X86, AddressModel::A_32))
    {
        OPTIONS_COMPILE += "/favor:blend";
        OPTIONS_COMPILE += GET_FLAG_EVALUATE(CpuType::EM64T, "/favor:EM64T", CpuType::AMD64, "/favor:AMD64");
    }
    if (AND(Threading::SINGLE, RuntimeLink::STATIC))
    {
        OPTIONS_COMPILE += GET_FLAG_EVALUATE(RuntimeDebugging::OFF, "/MT", RuntimeDebugging::ON, "/MTd");
    }
    LINKFLAGS_LINK += GET_FLAG_EVALUATE(Arch::IA64, "/MACHINE:IA64");
    if (EVALUATE(Arch::X86))
    {
        LINKFLAGS_LINK += GET_FLAG_EVALUATE(AddressModel::A_64, "/MACHINE:X64", AddressModel::A_32, "/MACHINE:X86");
    }
    else if (EVALUATE(Arch::ARM))
    {
        LINKFLAGS_LINK += GET_FLAG_EVALUATE(AddressModel::A_64, "/MACHINE:ARM64", AddressModel::A_32, "/MACHINE:ARM");
    }

    // Rule register-toolset-really on Line 1852
    SINGLE(RuntimeLink::SHARED, Threading::MULTI);
    // TODO
    // debug-store and pch-source features are being added. don't know where it will be used so holding back

    // TODO Line 1916 PCH Related Variables are not being set
    string PCH_FILE_COMPILE;
    string PCH_SOURCE_COMPILE;
    string PCH_HEADER_COMPILE;

    OPTIONS_COMPILE += GET_FLAG_EVALUATE(Optimization::SPEED, "/O2", Optimization::SPACE, "O1");
    // TODO:
    // Line 1927 - 1930 skipped because of cpu-type
    if (EVALUATE(Arch::IA64))
    {
        OPTIONS_COMPILE += GET_FLAG_EVALUATE(CpuType::ITANIUM, "/G1", CpuType::ITANIUM2, "/G2");
    }

    // Line 1930
    if (EVALUATE(DebugSymbols::ON))
    {
        OPTIONS_COMPILE += GET_FLAG_EVALUATE(DebugStore::OBJECT, "/Z7", DebugStore::DATABASE, "/Zi");
    }
    OPTIONS_COMPILE +=
        GET_FLAG_EVALUATE(Optimization::OFF, "/Od", Inlining::OFF, "/Ob0", Inlining::ON, "/Ob1", Inlining::FULL, "Ob2");
    OPTIONS_COMPILE +=
        GET_FLAG_EVALUATE(Warnings::ON, "/W3", Warnings::OFF, "/W0",
                          OR(Warnings::ALL, Warnings::EXTRA, Warnings::PEDANTIC), "/W4", WarningsAsErrors::ON, "/WX");
    string CPP_FLAGS_COMPILE;
    if (EVALUATE(ExceptionHandling::ON))
    {
        if (EVALUATE(AsyncExceptions::OFF))
        {
            CPP_FLAGS_COMPILE += GET_FLAG_EVALUATE(ExternCNoThrow::OFF, "/EHs", ExternCNoThrow::ON, "/EHsc");
        }
        else if (EVALUATE(AsyncExceptions::ON))
        {
            CPP_FLAGS_COMPILE += GET_FLAG_EVALUATE(ExternCNoThrow::OFF, "/EHa", ExternCNoThrow::ON, "EHac");
        }
    }
    CPP_FLAGS_COMPILE += GET_FLAG_EVALUATE(CxxSTD::V_14, "/std:c++14", CxxSTD::V_17, "/std:c++17", CxxSTD::V_20,
                                           "/std:c++20", CxxSTD::V_LATEST, "/std:c++latest");
    CPP_FLAGS_COMPILE += GET_FLAG_EVALUATE(RTTI::ON, "/GR", RTTI::OFF, "/GR-");
    if (EVALUATE(RuntimeLink::SHARED))
    {
        OPTIONS_COMPILE += GET_FLAG_EVALUATE(RuntimeDebugging::OFF, "/MD", RuntimeDebugging::ON, "/MDd");
    }
    else if (AND(RuntimeLink::STATIC, Threading::MULTI))
    {
        OPTIONS_COMPILE += GET_FLAG_EVALUATE(RuntimeDebugging::OFF, "/MT", RuntimeDebugging::ON, "/MTd");
    }
    string PDB_CFLAG = GET_FLAG_EVALUATE(AND(DebugSymbols::ON, DebugStore::DATABASE), "/Fd");

    // TODO// Line 1971
    //  There are variables UNDEFS and FORCE_INCLUDES

    string ASMFLAGS_ASM;
    if (EVALUATE(Arch::X86))
    {
        ASMFLAGS_ASM = GET_FLAG_EVALUATE(Warnings::ON, "/W3", Warnings::OFF, "/W0", Warnings::ALL, "/W4",
                                         WarningsAsErrors::ON, "/WX");
    }

    string PDB_LINKFLAG;
    string LINKFLAGS_MSVC;
    if (EVALUATE(DebugSymbols::ON))
    {
        PDB_LINKFLAG += GET_FLAG_EVALUATE(DebugStore::DATABASE, "/PDB:");
        LINKFLAGS_LINK += "/DEBUG ";
        LINKFLAGS_MSVC += GET_FLAG_EVALUATE(RuntimeDebugging::OFF, "/OPT:REF,ICF ");
    }
    LINKFLAGS_MSVC += GET_FLAG_EVALUATE(
        UserInterface::CONSOLE, "/subsystem:console", UserInterface::GUI, "/subsystem:windows", UserInterface::WINCE,
        "/subsystem:windowsce", UserInterface::NATIVE, "/subsystem:native", UserInterface::AUTO, "/subsystem:posix");

    // Line 1988
    //  Declare Flags for linking.
}

void CppSourceTarget::setPropertiesFlagsGCC()
{
    setTargetForAndOr();
    // TODO:
    //-Wl and --start-group options are needed because gcc command-line is used. But the HMake approach has been to
    // differentiate in compiler and linker
    //  Notes
    //   For Windows NT, we use a different JAMSHELL
    //   Currently, I ain't caring for the propagated properties. These properties are only being
    //   assigned to the parent.
    //   TODO s
    //   262 Be caustious of this rc-type. It has something to do with Windows resource compiler
    //   which I don't know
    //   TODO: flavor is being assigned based on the -dumpmachine argument to the gcc command. But this
    //    is not yet catered here.
    //

    // Calls to toolset.flags are being copied.
    // OPTIONS variable is being updated for rule gcc.compile and gcc.link action.
    string compilerFlags;
    string linkerFlags;
    string compilerAndLinkerFlags;
    // FINDLIBS-SA variable is being updated gcc.link rule.
    string findLibsSA;
    // OPTIONS variable is being updated for gcc.compile.c++ rule actions.
    string cppCompilerFlags;
    // DEFINES variable is being updated for gcc.compile.c++ rule actions.
    string cppDefineFlags;
    // OPTIONS variable is being updated for rule gcc.compile.c++ and gcc.link action.
    string cppCompilerAndLinkerFlags;
    // .IMPLIB-COMMAND variable is being updated for rule gcc.link.DLL action.
    string implibCommand;

    auto isTargetBSD = [&]() { return OR(TargetOS::BSD, TargetOS::FREEBSD, TargetOS::NETBSD, TargetOS::NETBSD); };
    {
        // -fPIC
        if (cTargetType == TargetType::LIBRARY_STATIC || cTargetType == TargetType::LIBRARY_SHARED)
        {
            if (targetOs != TargetOS::WINDOWS || targetOs != TargetOS::CYGWIN)
            {
            }
        }
    }
    {
        // Handle address-model
        if (targetOs == TargetOS::AIX && addModel == AddressModel::A_32)
        {
            compilerAndLinkerFlags += "-maix32";
        }
        if (targetOs == TargetOS::AIX && addModel == AddressModel::A_64)
        {
            compilerAndLinkerFlags += "-maix64";
        }
        if (targetOs == TargetOS::HPUX && addModel == AddressModel::A_32)
        {
            compilerAndLinkerFlags += "-milp32";
        }
        if (targetOs == TargetOS::HPUX && addModel == AddressModel::A_64)
        {
            compilerAndLinkerFlags += "-milp64";
        }
    }
    {
        // Handle threading
        if (EVALUATE(Threading::MULTI))
        {
            if (OR(TargetOS::WINDOWS, TargetOS::CYGWIN, TargetOS::SOLARIS))
            {
                compilerAndLinkerFlags += "-mthreads";
            }
            else if (targetOs == TargetOS::QNX || isTargetBSD())
            {
                compilerAndLinkerFlags += "-pthread";
            }
            else if (targetOs == TargetOS::SOLARIS)
            {
                compilerAndLinkerFlags += "-pthreads";
                findLibsSA += "rt";
            }
            else if (!OR(TargetOS::ANDROID, TargetOS::HAIKU, TargetOS::SGI, TargetOS::DARWIN, TargetOS::VXWORKS,
                         TargetOS::IPHONE, TargetOS::APPLETV))
            {
                compilerAndLinkerFlags += "-pthread";
                findLibsSA += "rt";
            }
        }
    }
    {
        CxxSTDDialect dCxxStdDialects = CxxSTDDialect::MS;
        auto setCppStdAndDialectCompilerAndLinkerFlags = [&](CxxSTD cxxStdLocal) {
            cppCompilerAndLinkerFlags += cxxStdDialect == CxxSTDDialect::GNU ? "-std=gnu++" : "-std=c++";
            CxxSTD temp = cxxStd;
            const_cast<CxxSTD &>(cxxStd) = cxxStdLocal;
            cppCompilerAndLinkerFlags += GET_FLAG_EVALUATE(
                CxxSTD::V_98, "98", CxxSTD::V_03, "03", CxxSTD::V_0x, "0x", CxxSTD::V_11, "11", CxxSTD::V_1y, "1y",
                CxxSTD::V_14, "14", CxxSTD::V_1z, "1z", CxxSTD::V_17, "17", CxxSTD::V_2a, "2a", CxxSTD::V_20, "20",
                CxxSTD::V_2b, "2b", CxxSTD::V_23, "23", CxxSTD::V_2c, "2c", CxxSTD::V_26, "26");
            const_cast<CxxSTD &>(cxxStd) = temp;
        };

        if (EVALUATE(CxxSTD::V_LATEST))
        {
            // Rule at Line 429
            /*            if (toolSet.version >= Version{10})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_20);
                        }
                        else if (toolSet.version >= Version{8})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_2a);
                        }
                        else if (toolSet.version >= Version{6})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_17);
                        }
                        else if (toolSet.version >= Version{5})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_1z);
                        }
                        else if (toolSet.version >= Version{4, 9})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_14);
                        }
                        else if (toolSet.version >= Version{4, 8})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_1y);
                        }
                        else if (toolSet.version >= Version{4, 7})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_11);
                        }
                        else if (toolSet.version >= Version{3, 3})
                        {
                            setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_98);
                        }*/
        }
        else
        {
            setCppStdAndDialectCompilerAndLinkerFlags(cxxStd);
        }
    }
    // From line 423 to line 625 as no library is using pch or obj

    // General options, link optimizations

    compilerFlags += GET_FLAG_EVALUATE(Optimization::OFF, "-O0", Optimization::SPEED, "-O3", Optimization::SPACE, "-Os",
                                       Optimization::MINIMAL, "-O1", Optimization::DEBUG, "-Og");

    compilerFlags += GET_FLAG_EVALUATE(Inlining::OFF, "-fno-inline", Inlining::ON, "-Wno-inline", Inlining::FULL,
                                       "-finline-functions -Wno-inline");

    compilerFlags += GET_FLAG_EVALUATE(Warnings::OFF, "-w", Warnings::ON, "-Wall", Warnings::ALL, "-Wall",
                                       Warnings::EXTRA, "-Wall -Wextra", Warnings::PEDANTIC, "-Wall -Wextra -pedantic");
    compilerFlags += GET_FLAG_EVALUATE(WarningsAsErrors::ON, "-Werror");

    compilerFlags += GET_FLAG_EVALUATE(DebugSymbols::ON, "-g");
    compilerFlags += GET_FLAG_EVALUATE(Profiling::ON, "-pg");

    compilerFlags += GET_FLAG_EVALUATE(LocalVisibility::HIDDEN, "-fvisibility=hidden");
    cppCompilerFlags += GET_FLAG_EVALUATE(LocalVisibility::HIDDEN, "-fvisibility-inlines-hidden");
    if (!EVALUATE(TargetOS::DARWIN))
    {
        compilerFlags += GET_FLAG_EVALUATE(LocalVisibility::PROTECTED, "-fvisibility=protected");
    }
    compilerFlags += GET_FLAG_EVALUATE(LocalVisibility::GLOBAL, "-fvisibility=default");

    cppCompilerFlags += GET_FLAG_EVALUATE(ExceptionHandling::OFF, "-fno-exceptions");
    cppCompilerFlags += GET_FLAG_EVALUATE(RTTI::OFF, "-fno-rtti");

    // Sanitizers
    string sanitizerFlags;
    sanitizerFlags += GET_FLAG_EVALUATE(AddressSanitizer::ON, "-fsanitize=address -fno-omit-frame-pointer",
                                        AddressSanitizer::NORECOVER,
                                        "-fsanitize=address -fno-sanitize-recover=address -fno-omit-frame-pointer");
    sanitizerFlags +=
        GET_FLAG_EVALUATE(LeakSanitizer::ON, "-fsanitize=leak -fno-omit-frame-pointer", LeakSanitizer::NORECOVER,
                          "-fsanitize=leak -fno-sanitize-recover=leak -fno-omit-frame-pointer");
    sanitizerFlags +=
        GET_FLAG_EVALUATE(ThreadSanitizer::ON, "-fsanitize=thread -fno-omit-frame-pointer", ThreadSanitizer::NORECOVER,
                          "-fsanitize=thread -fno-sanitize-recover=thread -fno-omit-frame-pointer");
    sanitizerFlags += GET_FLAG_EVALUATE(UndefinedSanitizer::ON, "-fsanitize=undefined -fno-omit-frame-pointer",
                                        UndefinedSanitizer::NORECOVER,
                                        "-fsanitize=undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer");
    sanitizerFlags += GET_FLAG_EVALUATE(Coverage::ON, "--coverage");

    cppCompilerFlags += sanitizerFlags;
    // I don't understand the following comment at line 667:
    // # configure Dinkum STL to match compiler options
    if (EVALUATE(TargetOS::VXWORKS))
    {
        cppDefineFlags += GET_FLAG_EVALUATE(RTTI::OFF, "_NO_RTTI");
        cppDefineFlags += GET_FLAG_EVALUATE(ExceptionHandling::OFF, "_NO_EX=1");
    }

    // LTO
    // compilerAndLinkerFlags += GET_FLAG_EVALUATE()
    if (EVALUATE(LTO::ON))
    {
        compilerAndLinkerFlags += GET_FLAG_EVALUATE(LTOMode::FULL, "-flto");
        compilerFlags += GET_FLAG_EVALUATE(LTOMode::FAT, "-flto -ffat-lto-objects");
        linkerFlags += GET_FLAG_EVALUATE(LTOMode::FAT, "-flto");
    }

    // ABI selection
    cppDefineFlags +=
        GET_FLAG_EVALUATE(StdLib::GNU, "_GLIBCXX_USE_CXX11_ABI=0", StdLib::GNU11, "_GLIBCXX_USE_CXX11_ABI=1");

    {
        bool noStaticLink = true;
        if (OR(TargetOS::WINDOWS, TargetOS::VMS))
        {
            noStaticLink = false;
        }
        if (noStaticLink && EVALUATE(RuntimeLink::STATIC))
        {
            if (EVALUATE(Link::SHARED))
            {
                print("WARNING: On gcc, DLLs can not be built with <runtime-link>static\n");
            }
            else
            {
                // TODO:  Line 718
                //  Implement the remaining rule
            }
        }
    }

    // Linker Flags

    linkerFlags += GET_FLAG_EVALUATE(DebugSymbols::ON, "-g");
    linkerFlags += GET_FLAG_EVALUATE(Profiling::ON, "-pg");

    linkerFlags += GET_FLAG_EVALUATE(LocalVisibility::HIDDEN, "-fvisibility=hidden -fvisibility-inlines-hidden",
                                     LocalVisibility::GLOBAL, "-fvisibility=default");
    if (!EVALUATE(TargetOS::DARWIN))
    {
        linkerFlags += GET_FLAG_EVALUATE(LocalVisibility::PROTECTED, "-fvisibility=protected");
    }

    // Sanitizers
    // Though sanitizer flags for compiler are assigned at different location, and are for c++, but are exact same
    linkerFlags += sanitizerFlags;
    linkerFlags += GET_FLAG_EVALUATE(Coverage::ON, "--coverage");

    // Target Specific Flags
    // AIX
    /* On AIX we *have* to use the native linker.

   The -bnoipath strips the prepending (relative) path of libraries from
   the loader section in the target library or executable. Hence, during
   load-time LIBPATH (identical to LD_LIBRARY_PATH) or a hard-coded
   -blibpath (*similar* to -lrpath/-lrpath-link) is searched. Without
   this option, the prepending (relative) path + library name is
   hard-coded in the loader section, causing *only* this path to be
   searched during load-time. Note that the AIX linker does not have an
   -soname equivalent, this is as close as it gets.

   The -bbigtoc option instrcuts the linker to create a TOC bigger than 64k.
   This is necessary for some submodules such as math, but it does make running
   the tests a tad slower.

   The above options are definitely for AIX 5.x, and most likely also for
   AIX 4.x and AIX 6.x. For details about the AIX linker see:
   http://download.boulder.ibm.com/ibmdl/pub/software/dw/aix/es-aix_ll.pdf
   */
    linkerFlags += GET_FLAG_EVALUATE(TargetOS::AIX, "-Wl -bnoipath -Wl -bbigtoc");
    linkerFlags += AND(TargetOS::AIX, RuntimeLink::STATIC) ? "-static" : "";
    //  On Darwin, the -s option to ld does not work unless we pass -static,
    // and passing -static unconditionally is a bad idea. So, do not pass -s
    // at all and darwin.jam will use a separate 'strip' invocation.

    // 866
}

void CppSourceTarget::initializeForBuild()
{
    // string fileName = path(getTargetPointer()).filename().string();
    bTargetType = cTargetType;

    auto parseRegexSourceDirs = [target = this](bool assignToSourceNodes) {
        for (const SourceDirectory &srcDir : assignToSourceNodes ? target->regexSourceDirs : target->regexModuleDirs)
        {
            for (const auto &k : recursive_directory_iterator(path(srcDir.sourceDirectory->filePath)))
            {
                try
                {
                    if (k.is_regular_file() && regex_match(k.path().filename().string(), std::regex(srcDir.regex)))
                    {
                        if (assignToSourceNodes)
                        {
                            target->sourceFileDependencies.emplace(target, k.path().generic_string(),
                                                                   ResultType::SOURCENODE);
                        }
                        else
                        {
                            target->moduleSourceFileDependencies.emplace(target, k.path().generic_string());
                        }
                    }
                }
                catch (const std::regex_error &e)
                {
                    print(stderr, "regex_error : {}\nError happened while parsing regex {} of target{}\n", e.what(),
                          srcDir.regex, target->getTargetPointer());
                    exit(EXIT_FAILURE);
                }
            }
        }
    };
    parseRegexSourceDirs(true);
    parseRegexSourceDirs(false);
    buildCacheFilesDirPath = (path(targetFileDir) / ("Cache_Build_Files/")).generic_string();
    // Parsing finished

    auto [pos, Ok] = variantFilePaths.emplace(path(getTargetPointer()).parent_path().parent_path().string() + "/" +
                                              "projectVariant.hmake");
    variantFilePath = &(*pos);

    // TODO
    /*    if (!sourceFiles.empty() && !modulesSourceFiles.empty())
        {
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                  "A Target can not have both module-source and regular-source. You "
                  "can make regular-source dependency "
                  "of module source.\nTarget: {}.\n",
                  getTargetPointer());
            exit(EXIT_FAILURE);
        }*/
}

void CppSourceTarget::setJson()
{
    json[JConsts::targetType] = cTargetType;
    json[JConsts::configuration] = configurationType;
    json[JConsts::compiler] = compiler;
    json[JConsts::linker] = linker;
    json[JConsts::archiver] = archiver;
    json[JConsts::compilerFlags] = publicCompilerFlags;
    // json[JConsts::linkerFlags] = publicLinkerFlags;
    string str = "SOURCE_";
    json[str + JConsts::files] = sourceFileDependencies;
    json[str + JConsts::directories] = regexSourceDirs;
    str = "MODULE_";
    json[str + JConsts::files] = moduleSourceFileDependencies;
    json[str + JConsts::directories] = regexModuleDirs;
    json[JConsts::includeDirectories] = publicIncludes;
    json[JConsts::huIncludeDirectories] = publicHUIncludes;
    // TODO
    json[JConsts::compilerTransitiveFlags] = publicCompilerFlags;
    // json[JConsts::linkerTransitiveFlags] = publicLinkerFlags;
    json[JConsts::compileDefinitions] = publicCompileDefinitions;
    json[JConsts::variant] = JConsts::project;
    /*    if (moduleScope)
            {
                targetFileJson[JConsts::moduleScope] = moduleScope->getTargetFilePath(configurationName);
            }
            else
            {
                print(stderr, "Module Scope not assigned\n");
            }
            if (configurationScope)
            {
                targetFileJson[JConsts::configurationScope] = configurationScope->getTargetFilePath(configurationName);
            }
            else
            {
                print(stderr, "Configuration Scope not assigned\n");
            }*/
}

void CppSourceTarget::writeJsonFile()
{
    // In case of other being LinkOrArchiveTarget and this not having file, this does not emplace the Json at end
    // because setJson of LinkOrArchiveTarget adds the Json
    if (auto *target = dynamic_cast<LinkOrArchiveTarget *>(other); target)
    {
        if (!hasFile)
        {
            return;
        }
    }
    CTarget::writeJsonFile();
}

BTarget *CppSourceTarget::getBTarget()
{
    return this;
}

CppSourceTarget &CppSourceTarget::PUBLIC_COMPILER_FLAGS(const string &compilerFlags)
{
    publicCompilerFlags += compilerFlags;
    return *this;
}

CppSourceTarget &CppSourceTarget::PRIVATE_COMPILER_FLAGS(const string &compilerFlags)
{
    privateCompilerFlags += compilerFlags;
    return *this;
}

CppSourceTarget &CppSourceTarget::PUBLIC_LINKER_FLAGS(const string &linkerFlags)
{
    // publicLinkerFlags += linkerFlags;
    return *this;
}

CppSourceTarget &CppSourceTarget::PRIVATE_LINKER_FLAGS(const string &linkerFlags)
{
    privateLinkerFlags += linkerFlags;
    return *this;
}

CppSourceTarget &CppSourceTarget::PUBLIC_COMPILE_DEFINITION(const string &cddName, const string &cddValue)
{
    publicCompileDefinitions.emplace_back(cddName, cddValue);
    return *this;
}

CppSourceTarget &CppSourceTarget::PRIVATE_COMPILE_DEFINITION(const string &cddName, const string &cddValue)
{
    publicCompileDefinitions.emplace_back(cddName, cddValue);
    return *this;
}

CppSourceTarget &CppSourceTarget::SOURCE_DIRECTORIES(const string &sourceDirectory, const string &regex)
{
    regexSourceDirs.emplace(sourceDirectory, regex);
    return *this;
}

CppSourceTarget &CppSourceTarget::MODULE_DIRECTORIES(const string &moduleDirectory, const string &regex)
{
    regexModuleDirs.emplace(moduleDirectory, regex);
    return *this;
}

string &CppSourceTarget::getCompileCommand()
{
    if (compileCommand.empty())
    {
        setCompileCommand();
    }
    return compileCommand;
}

void CppSourceTarget::setCompileCommand()
{
    auto getIncludeFlag = [this]() {
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            return "/I ";
        }
        else
        {
            return "-I ";
        }
    };

    compileCommand += publicCompilerFlags + " ";
    compileCommand += compilerTransitiveFlags + " ";

    for (const auto &i : publicCompileDefinitions)
    {
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            compileCommand += "/D" + i.name + "=" + i.value + " ";
        }
        else
        {
            compileCommand += "-D" + i.name + "=" + i.value + " ";
        }
    }

    for (const Node *idd : publicIncludes)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(idd->filePath) + " ");
    }
    for (const Node *idd : publicHUIncludes)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(idd->filePath) + " ");
    }

    for (const string &str : includeDirectories)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(str) + " ");
    }
}

void CppSourceTarget::setSourceCompileCommandPrintFirstHalf()
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    auto getIncludeFlag = [this]() {
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            return "/I ";
        }
        else
        {
            return "-I ";
        }
    };

    if (ccpSettings.tool.printLevel != PathPrintLevel::NO)
    {
        sourceCompileCommandPrintFirstHalf +=
            getReducedPath(compiler.bTPath.make_preferred().string(), ccpSettings.tool) + " ";
    }

    if (ccpSettings.compilerFlags)
    {
        sourceCompileCommandPrintFirstHalf += publicCompilerFlags + " ";
    }
    if (ccpSettings.compilerTransitiveFlags)
    {
        sourceCompileCommandPrintFirstHalf += compilerTransitiveFlags + " ";
    }

    for (const auto &i : publicCompileDefinitions)
    {
        if (ccpSettings.compileDefinitions)
        {
            if (compiler.bTFamily == BTFamily::MSVC)
            {
                sourceCompileCommandPrintFirstHalf += "/D " + addQuotes(i.name + "=" + addQuotes(i.value)) + " ";
            }
            else
            {
                sourceCompileCommandPrintFirstHalf += "-D" + i.name + "=" + i.value + " ";
            }
        }
    }

    for (const Node *idd : publicIncludes)
    {
        if (ccpSettings.projectIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            sourceCompileCommandPrintFirstHalf +=
                getIncludeFlag() + getReducedPath(idd->filePath, ccpSettings.projectIncludeDirectories) + " ";
        }
    }
    for (const Node *idd : publicHUIncludes)
    {
        if (ccpSettings.projectIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            sourceCompileCommandPrintFirstHalf +=
                getIncludeFlag() + getReducedPath(idd->filePath, ccpSettings.projectIncludeDirectories) + " ";
        }
    }

    for (const string &str : includeDirectories)
    {
        if (ccpSettings.environmentIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            sourceCompileCommandPrintFirstHalf +=
                getIncludeFlag() + getReducedPath(str, ccpSettings.environmentIncludeDirectories) + " ";
        }
    }
}

string &CppSourceTarget::getSourceCompileCommandPrintFirstHalf()
{
    if (sourceCompileCommandPrintFirstHalf.empty())
    {
        setSourceCompileCommandPrintFirstHalf();
    }
    return sourceCompileCommandPrintFirstHalf;
}

void CppSourceTarget::setSourceNodeFileStatus(SourceNode &sourceNode, bool angle = false) const
{
    sourceNode.fileStatus = FileStatus::UPDATED;

    path objectFilePath = path(buildCacheFilesDirPath + path(sourceNode.node->filePath).filename().string() + ".o");

    if (!std::filesystem::exists(objectFilePath))
    {
        sourceNode.fileStatus = FileStatus::NEEDS_UPDATE;
        return;
    }
    file_time_type objectFileLastEditTime = last_write_time(objectFilePath);
    if (sourceNode.node->getLastUpdateTime() > objectFileLastEditTime)
    {
        sourceNode.fileStatus = FileStatus::NEEDS_UPDATE;
    }
    else
    {
        for (const Node *headerNode : sourceNode.headerDependencies)
        {
            if (headerNode->getLastUpdateTime() > objectFileLastEditTime)
            {
                sourceNode.fileStatus = FileStatus::NEEDS_UPDATE;
                break;
            }
        }
    }
}

void CppSourceTarget::checkForPreBuiltAndCacheDir()
{
    if (!linkOrArchiveTarget)
    {
        // TODO
    }
}

void CppSourceTarget::checkForPreBuiltAndCacheDir(const Json &sourceCacheJson)
{
    setCompileCommand();
    string str = sourceCacheJson.at(JConsts::compileCommand).get<string>();
    // linkCommand = targetCacheJson.at(JConsts::linkCommand).get<string>();
    auto initializeSourceNodePointer = [](SourceNode *sourceNode, const Json &j) {
        for (const Json &headerFile : j.at(JConsts::headerDependencies))
        {
            sourceNode->headerDependencies.emplace(Node::getNodeFromString(headerFile, true));
        }
        sourceNode->presentInCache = true;
    };
    if (str == compileCommand)
    {
        for (const Json &j : sourceCacheJson.at(JConsts::sourceDependencies))
        {
            initializeSourceNodePointer(
                const_cast<SourceNode *>(
                    sourceFileDependencies.emplace(this, j.at(JConsts::srcFile), ResultType::SOURCENODE)
                        .first.
                        operator->()),
                j);
        }
        for (const Json &j : sourceCacheJson.at(JConsts::moduleDependencies))
        {
            initializeSourceNodePointer(
                const_cast<SMFile *>(
                    moduleSourceFileDependencies.emplace(this, j.at(JConsts::srcFile)).first.operator->()),
                j);
        }
    }
}

void CppSourceTarget::populateSetTarjanNodesSourceNodes(Builder &builder)
{
    for (const SourceNode &sourceNodeConst : sourceFileDependencies)
    {
        auto &sourceNode = const_cast<SourceNode &>(sourceNodeConst);
        if (sourceNode.presentInSource != sourceNode.presentInCache)
        {
            sourceNode.fileStatus = FileStatus::NEEDS_UPDATE;
        }
        else
        {
            setSourceNodeFileStatus(sourceNode);
        }
        if (linkOrArchiveTarget)
        {
            linkOrArchiveTarget->addDependency(sourceNode);
        }
        else
        {
            addDependency(sourceNode);
        }
    }
}

template <typename T, typename U = std::less<T>> pair<T *, bool> insert(set<T *, U> &containerSet, T *element)
{
    auto [pos, Ok] = containerSet.emplace(element);
    return make_pair(const_cast<T *>(*pos), Ok);
}

template <typename T, typename U = std::less<T>, typename... V>
pair<T *, bool> insert(set<T, U> &containerSet, V &&...elements)
{
    auto [pos, Ok] = containerSet.emplace(std::forward<V>(elements)...);
    return make_pair(const_cast<T *>(&(*pos)), Ok);
}

void CppSourceTarget::parseModuleSourceFiles(Builder &builder)
{
    ModuleScope &moduleScope = builder.moduleScopes.emplace(variantFilePath, ModuleScope{}).first->second;
    for (const Node *idd : publicHUIncludes)
    {
        if (const auto &[pos, Ok] = moduleScope.appHUDirTarget.emplace(idd, this); !Ok)
        {
            // TODO:
            //  Improve Message
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                  "hu-include-directory\n{}\n is being provided by two different targets\n{}\n{}\nThis is not allowed "
                  "because HMake can't determine which Header Unit to attach to which target.",
                  idd->filePath, getTargetPointer(), pos->second->getTargetPointer());
            exit(EXIT_FAILURE);
        }
    }
    for (const SMFile &smFileConst : moduleSourceFileDependencies)
    {
        auto smFile = const_cast<SMFile &>(smFileConst);

        if (auto [pos, Ok] = moduleScope.smFiles.emplace(&smFile); Ok)
        {
            if (smFile.presentInSource != smFile.presentInCache)
            {
                smFile.fileStatus = FileStatus::NEEDS_UPDATE;
            }
            else
            {
                // TODO: Audit the code
                //  This will check if the smfile of the module source file is outdated.
                //  If it is then scan-dependencies operation is performed on the module
                //  source.
                setSourceNodeFileStatus(smFile);
            }
            if (smFile.fileStatus == FileStatus::NEEDS_UPDATE)
            {
                smFile.resultType = ResultType::CPP_SMFILE;
                builder.finalBTargets.emplace_back(&smFile);
            }
        }
        else
        {
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                  "In Module Scope\n{}\nmodule file\n{}\nis being provided by two targets\n{}\n{}\n", *variantFilePath,
                  smFile.node->filePath, getTargetPointer(), (**pos).target->getTargetPointer());
            exit(EXIT_FAILURE);
        }
    }
}

string CppSourceTarget::getInfrastructureFlags()
{
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        return "/showIncludes /c /Tp";
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
        // Will like to use -MD but not using it currently because sometimes it
        // prints 2 header deps in one line and no space in them so no way of
        // knowing whether this is a space in path or 2 different headers. Which
        // then breaks when last_write_time is checked for that path.
        return "-c -MMD";
    }
    return "";
}

string CppSourceTarget::getCompileCommandPrintSecondPart(const SourceNode &sourceNode)
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;

    string command;
    if (ccpSettings.infrastructureFlags)
    {
        command += getInfrastructureFlags() + " ";
    }
    if (ccpSettings.sourceFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(sourceNode.node->filePath, ccpSettings.sourceFile) + " ";
    }
    if (ccpSettings.infrastructureFlags)
    {
        command += compiler.bTFamily == BTFamily::MSVC ? "/Fo" : "-o ";
    }
    if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(buildCacheFilesDirPath + path(sourceNode.node->filePath).filename().string() + ".o",
                                  ccpSettings.objectFile) +
                   " ";
    }
    return command;
}

PostCompile CppSourceTarget::CompileSMFile(SMFile &smFile)
{
    // TODO: Add Compiler error handling for this operation.
    // TODO: A temporary hack to support modules
    string finalCompileCommand = compileCommand;

    for (const BTarget *bTarget : smFile.allDependencies)
    {
        if (bTarget->resultType == ResultType::CPP_MODULE)
        {
            const auto *smFileDependency = static_cast<const SMFile *>(bTarget);
            finalCompileCommand += smFileDependency->getRequireFlag(smFile) + " ";
        }
    }
    finalCompileCommand += getInfrastructureFlags() + addQuotes(smFile.node->filePath) + " ";

    // TODO:
    //  getFlag and getRequireFlags create confusion. Instead of getRequireFlags, only getFlag should be used.
    if (smFile.type == SM_FILE_TYPE::HEADER_UNIT)
    {
        if (smFile.standardHeaderUnit)
        {
            string cacheFilePath = (path(*(smFile.target->variantFilePath)).parent_path() / "shus/").generic_string();
            finalCompileCommand += smFile.getFlag(cacheFilePath + path(smFile.node->filePath).filename().string());
        }
        else
        {
            finalCompileCommand += smFile.getFlag(smFile.ahuTarget->buildCacheFilesDirPath +
                                                  path(smFile.node->filePath).filename().string());
        }
    }
    else
    {
        finalCompileCommand += smFile.getFlag(buildCacheFilesDirPath + path(smFile.node->filePath).filename().string());
    }

    return PostCompile{*this,
                       compiler,
                       finalCompileCommand,
                       getSourceCompileCommandPrintFirstHalf() + smFile.getModuleCompileCommandPrintLastHalf(),
                       buildCacheFilesDirPath,
                       path(smFile.node->filePath).filename().string(),
                       settings.ccpSettings.outputAndErrorFiles};
}

PostCompile CppSourceTarget::Compile(SourceNode &sourceNode)
{
    string compileFileName = path(sourceNode.node->filePath).filename().string();

    string finalCompileCommand = getCompileCommand() + " ";

    finalCompileCommand += getInfrastructureFlags() + " " + addQuotes(sourceNode.node->filePath) + " ";
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        finalCompileCommand += "/Fo" + addQuotes(buildCacheFilesDirPath + compileFileName + ".o") + " ";
    }
    else
    {
        finalCompileCommand += "-o " + addQuotes(buildCacheFilesDirPath + compileFileName + ".o") + " ";
    }

    return PostCompile{*this,
                       compiler,
                       finalCompileCommand,
                       getSourceCompileCommandPrintFirstHalf() + getCompileCommandPrintSecondPart(sourceNode),
                       buildCacheFilesDirPath,
                       compileFileName,
                       settings.ccpSettings.outputAndErrorFiles};
}

PostBasic CppSourceTarget::GenerateSMRulesFile(const SourceNode &sourceNode, bool printOnlyOnError)
{
    string finalCompileCommand = getCompileCommand() + addQuotes(sourceNode.node->filePath) + " ";

    if (compiler.bTFamily == BTFamily::MSVC)
    {
        finalCompileCommand +=
            " /scanDependencies " +
            addQuotes(buildCacheFilesDirPath + path(sourceNode.node->filePath).filename().string() + ".smrules") + " ";
    }
    else
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "Modules supported only on MSVC");
    }

    return printOnlyOnError
               ? PostBasic(compiler, finalCompileCommand, "", buildCacheFilesDirPath,
                           path(sourceNode.node->filePath).filename().string(),
                           settings.ccpSettings.outputAndErrorFiles, true)
               : PostBasic(compiler, finalCompileCommand,
                           getSourceCompileCommandPrintFirstHalf() + getCompileCommandPrintSecondPart(sourceNode),
                           buildCacheFilesDirPath, path(sourceNode.node->filePath).filename().string() + ".smrules",
                           settings.ccpSettings.outputAndErrorFiles, true);
}

TargetType CppSourceTarget::getTargetType() const
{
    return bTargetType;
}

void CppSourceTarget::pruneAndSaveBuildCache(const bool successful)
{
    ofstream(path(buildCacheFilesDirPath) / (name + ".cache")) << getBuildCache(successful).dump(4);
}

Json CppSourceTarget::getBuildCache(const bool successful)
{
    // TODO:
    // Not dealing with module source
    for (auto it = sourceFileDependencies.begin(); it != sourceFileDependencies.end();)
    {
        !it->presentInSource ? it = sourceFileDependencies.erase(it) : ++it;
    }
    if (!successful)
    {
        for (auto it = sourceFileDependencies.begin(); it != sourceFileDependencies.end();)
        {
            it->fileStatus == FileStatus::NEEDS_UPDATE ? it = sourceFileDependencies.erase(it) : ++it;
        }
    }
    Json buildCache;
    buildCache[JConsts::compileCommand] = compileCommand;
    buildCache[JConsts::sourceDependencies] = sourceFileDependencies;
    buildCache[JConsts::moduleDependencies] = moduleSourceFileDependencies;
    return buildCache;
}

Executable::Executable(string targetName_, const bool initializeFromCache)
    : CppSourceTarget(std::move(targetName_), initializeFromCache)
{
    cTargetType = TargetType::EXECUTABLE;
}

Executable::Executable(string targetName_, Variant &variant) : CppSourceTarget(std::move(targetName_), variant)
{
    cTargetType = TargetType::EXECUTABLE;
}

Library::Library(string targetName_, const bool initializeFromCache)
    : CppSourceTarget(std::move(targetName_), initializeFromCache)
{
    cTargetType = libraryType;
}

Library::Library(string targetName_, Variant &variant) : CppSourceTarget(std::move(targetName_), variant)
{
    cTargetType = libraryType;
}

void to_json(Json &j, const Library *library)
{
    j[JConsts::prebuilt] = false;
    j[JConsts::path] = library->getTargetPointer();
}

bool operator==(const Version &lhs, const Version &rhs)
{
    return lhs.majorVersion == rhs.majorVersion && lhs.minorVersion == rhs.minorVersion &&
           lhs.patchVersion == rhs.patchVersion;
}

PLibrary::PLibrary(Variant &variant, const path &libraryPath_, TargetType libraryType_)
{
    // TODO
    // Should check if library exists or not
    libraryType = libraryType_;
    if (libraryType != TargetType::PLIBRARY_STATIC || libraryType != TargetType::PLIBRARY_SHARED)
    {
        print(stderr, "PLibrary libraryType TargetType is not one of PLIBRARY_STATIC or PLIBRARY_SHARED \n");
        exit(EXIT_FAILURE);
    }
    variant.preBuiltLibraries.emplace(this);
    if (libraryPath_.is_relative())
    {
        libraryPath = path(srcDir) / libraryPath_;
        libraryPath = libraryPath.lexically_normal();
    }
    libraryName = getTargetNameFromActualName(libraryType, os, libraryPath.filename().string());
}

bool operator<(const PLibrary &lhs, const PLibrary &rhs)
{
    // TODO
    // What is this?
    return lhs.libraryName < rhs.libraryName && lhs.libraryPath < rhs.libraryPath && lhs.includes < rhs.includes &&
           lhs.compilerFlags < rhs.compilerFlags && lhs.compilerFlags < rhs.compilerFlags;
}

void to_json(Json &j, const PLibrary *pLibrary)
{
    j[JConsts::prebuilt] = true;
    j[JConsts::path] = pLibrary->libraryPath.generic_string();
}