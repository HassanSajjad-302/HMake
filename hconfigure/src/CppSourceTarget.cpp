
#ifdef USE_HEADER_UNITS
import "CppSourceTarget.hpp";
import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "Cache.hpp";
import "LinkOrArchiveTarget.hpp";
import "Utilities.hpp";
import <filesystem>;
import <fstream>;
import <regex>;
import <utility>;
#else
#include "CppSourceTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "Cache.hpp"
#include "LinkOrArchiveTarget.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <fstream>
#include <regex>
#include <utility>
#endif

using std::filesystem::create_directories, std::filesystem::directory_iterator,
    std::filesystem::recursive_directory_iterator, std::ifstream, std::ofstream, std::regex, std::regex_error;

SourceDirectory::SourceDirectory(const string &sourceDirectory_, string regex_, const bool recursive_)
    : sourceDirectory{Node::getNodeFromString(sourceDirectory_, false)}, regex{std::move(regex_)}, recursive(recursive_)
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

SourceNode &CppSourceTarget::addNodeInSourceFileDependencies(Node *node)
{
    set<SourceNode, CompareSourceNode>::const_iterator sourceNode;
    if (auto it = sourceFileDependencies.find(node); it == sourceFileDependencies.end())
    {
        sourceNode = sourceFileDependencies.emplace(this, node).first;
    }
    else
    {
        sourceNode = it;
    }
    return const_cast<SourceNode &>(*sourceNode);
}

SMFile &CppSourceTarget::addNodeInModuleSourceFileDependencies(Node *node)
{
    set<SMFile, CompareSourceNode>::const_iterator moduleNode;
    if (auto it = moduleSourceFileDependencies.find(node); it == moduleSourceFileDependencies.end())
    {
        moduleNode = moduleSourceFileDependencies.emplace(this, node).first;
    }
    else
    {
        moduleNode = it;
    }
    return const_cast<SMFile &>(*moduleNode);
}

SMFile &CppSourceTarget::addNodeInHeaderUnits(Node *node)
{
    set<SMFile, CompareSourceNode>::const_iterator headerUnit;
    if (auto it = moduleScopeData->headerUnits.find(node); it == moduleScopeData->headerUnits.end())
    {
        headerUnit = moduleScopeData->headerUnits.emplace(this, node).first;
        headerUnits.emplace(&(*headerUnit));
    }
    else
    {
        headerUnit = it;
    }
    return const_cast<SMFile &>(*headerUnit);
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

CppSourceTarget::CppSourceTarget(string name_, TargetType targetType) : CTarget{std::move(name_)}
{
    compileTargetType = targetType;
}

CppSourceTarget::CppSourceTarget(string name_, TargetType targetType, CTarget &other, bool hasFile)
    : CTarget{std::move(name_), other, hasFile}
{
    compileTargetType = targetType;
}

void CppSourceTarget::getObjectFiles(vector<const ObjectFile *> *objectFiles,
                                     LinkOrArchiveTarget *linkOrArchiveTarget) const
{
    set<const SMFile *, IndexInTopologicalSortComparatorRoundZero> sortedSMFileDependencies;
    for (const SMFile &objectFile : moduleSourceFileDependencies)
    {
        if (objectFile.presentInSource)
        {
            sortedSMFileDependencies.emplace(&objectFile);
        }
    }
    for (const SMFile *headerUnit : headerUnits)
    {
        if (headerUnit->presentInSource)
        {
            sortedSMFileDependencies.emplace(headerUnit);
        }
    }

    for (const SMFile *objectFile : sortedSMFileDependencies)
    {
        objectFiles->emplace_back(objectFile);
    }

    for (const SourceNode &objectFile : sourceFileDependencies)
    {
        if (objectFile.presentInSource)
        {
            objectFiles->emplace_back(&objectFile);
        }
    }

    for (const ObjectFileProducer *objectFileTarget : requirementObjectFileTargets)
    {
        objectFileTarget->getObjectFiles(objectFiles, linkOrArchiveTarget);
    }
}

void CppSourceTarget::addRequirementDepsToBTargetDependencies()
{
    // Access to addDependency() function must be synchronized
    std::lock_guard<std::mutex> lk(BTargetNamespace::addDependencyMutex);

    for (CPT *cppSourceTarget : requirementDeps)
    {
        getRealBTarget(2).addDependency(const_cast<CPT &>(*cppSourceTarget));
    }
}

void CppSourceTarget::populateTransitiveProperties()
{
    for (CPT *cppSourceTarget : requirementDeps)
    {
        for (InclNode &include : cppSourceTarget->usageRequirementIncludes)
        {
            InclNode::emplaceInList(requirementIncludes, include);
        }
        requirementCompilerFlags += cppSourceTarget->usageRequirementCompilerFlags;
        for (const Define &define : cppSourceTarget->usageRequirementCompileDefinitions)
        {
            requirementCompileDefinitions.emplace(define);
        }
        requirementCompilerFlags += cppSourceTarget->usageRequirementCompilerFlags;
        for (const ObjectFileProducer *objectFileProducer : cppSourceTarget->usageRequirementObjectFileProducers)
        {
            requirementObjectFileTargets.emplace(objectFileProducer);
        }
    }
}

CppSourceTarget &CppSourceTarget::setModuleScope(CppSourceTarget *moduleScope_)
{
    moduleScope = moduleScope_;
    return *this;
}

CppSourceTarget &CppSourceTarget::assignStandardIncludesToHUIncludes()
{
    for (InclNode &include : requirementIncludes)
    {
        if (include.isStandard)
        {
            huIncludes.emplace_back(&include);
        }
    }
    return *this;
}

// For some features the resultant object-file is same these are termed incidental. Change these does not result in
// recompilation. Skip these in compiler-command that is cached.
CompilerFlags CppSourceTarget::getCompilerFlags()
{
    CompilerFlags flags;
    if (compiler.bTFamily == BTFamily::MSVC)
    {

        // msvc.jam supports multiple tools such as assembler, compiler, mc-compiler(message-catalogue-compiler),
        // idl-compiler(interface-definition-compiler) and manifest-tool. HMake does not support these and  only
        // supports link.exe, lib.exe and cl.exe. While the msvc.jam also supports older VS and store and phone Windows
        // API, HMake only supports the recent Visual Studio version and the Desktop API. Besides these limitation,
        // msvc.jam is tried to be imitated here.

        // Hassan Sajjad
        // I don't have complete confidence about correctness of following info.

        // On Line 2220, auto-detect-toolset-versions calls register-configuration. This call version.set with the
        // version and compiler path(obtained from default-path rule). After that, register toolset registers all
        // generators. And once msvc toolset is init, configure-relly is called that sets the setup script of previously
        // set .version/configuration. setup-script captures all of environment-variables set when vcvarsall.bat for a
        // configuration is run in file "msvc-setup.bat" file. This batch file run before the generator actions. This is
        // .SETUP variable in actions.

        // all variables in actions are in CAPITALS

        // Line 1560
        string defaultAssembler = EVALUATE(Arch::IA64) ? "ias " : "";
        if (EVALUATE(Arch::X86))
        {
            defaultAssembler += GET_FLAG_EVALUATE(AddressModel::A_64, "ml64 ", AddressModel::A_32, "ml -coff ");
        }
        else if (EVALUATE(Arch::ARM))
        {
            defaultAssembler += GET_FLAG_EVALUATE(AddressModel::A_64, "armasm64 ", AddressModel::A_32, "armasm ");
        }
        string assemblerFlags = GET_FLAG_EVALUATE(OR(Arch::X86, Arch::IA64), "-c -Zp4 -Cp -Cx ");
        string assemblerOutputFlag = GET_FLAG_EVALUATE(OR(Arch::X86, Arch::IA64), "-Fo ", Arch::ARM, "-o ");
        // Line 1618

        // Line 1650
        flags.DOT_CC_COMPILE += "/Zm800 -nologo ";
        flags.DOT_ASM_COMPILE += defaultAssembler + assemblerFlags + "-nologo ";
        flags.DOT_ASM_OUTPUT_COMPILE += assemblerOutputFlag;
        flags.DOT_LD_ARCHIVE += "lib /NOLOGO ";

        // Line 1670
        flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(LTO::ON, "/GL ");
        // End-Line 1682

        // Function completed. Jumping to rule configure-version-specific.
        // Line 444
        // Only flags effecting latest MSVC tools (14.3) are supported.

        flags.OPTIONS_COMPILE += "/Zc:forScope /Zc:wchar_t ";
        flags.CPP_FLAGS_COMPILE_CPP = "/wd4675 ";
        flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(Warnings::OFF, "/wd4996 ");
        flags.OPTIONS_COMPILE += "/Zc:inline ";
        flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(OR(Optimization::SPEED, Optimization::SPACE), "/Gw ");
        flags.CPP_FLAGS_COMPILE_CPP += "/Zc:throwingNew ";

        // Line 492
        flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(AddressSanitizer::ON, "/fsanitize=address /FS ");

        if (EVALUATE(AddressModel::A_64))
        {
            // The various 64 bit runtime asan support libraries and related flags.
            string FINDLIBS_SA_LINK =
                GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::SHARED),
                                  "clang_rt.asan_dynamic-x86_64 clang_rt.asan_dynamic_runtime_thunk-x86_64 ");
            FINDLIBS_SA_LINK +=
                GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC, TargetType::EXECUTABLE),
                                  "clang_rt.asan-x86_64 clang_rt.asan_cxx-x86_64 ");
            string FINDLIBS_SA_LINK_DLL =
                GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC), "clang_rt.asan_dll_thunk-x86_64 ");
            string LINKFLAGS_LINK_DLL = GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC),
                                                          R"(/wholearchive\:"clang_rt.asan_dll_thunk-x86_64.lib ")");
        }
        else if (EVALUATE(AddressModel::A_32))
        {
            // The various 32 bit runtime asan support libraries and related flags.

            string FINDLIBS_SA_LINK =
                GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::SHARED),
                                  "clang_rt.asan_dynamic-i386 clang_rt.asan_dynamic_runtime_thunk-i386 ");
            FINDLIBS_SA_LINK +=
                GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC, TargetType::EXECUTABLE),
                                  "clang_rt.asan-i386 clang_rt.asan_cxx-i386 ");
            string FINDLIBS_SA_LINK_DLL =
                GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC), "clang_rt.asan_dll_thunk-i386 ");
            string LINKFLAGS_LINK_DLL = GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC),
                                                          R"(/wholearchive\:"clang_rt.asan_dll_thunk-i386.lib ")");
        }

        // Line 586
        if (AND(Arch::X86, AddressModel::A_32))
        {
            flags.OPTIONS_COMPILE += "/favor:blend ";
            flags.OPTIONS_COMPILE +=
                GET_FLAG_EVALUATE(CpuType::EM64T, "/favor:EM64T ", CpuType::AMD64, "/favor:AMD64 ");
        }
        if (AND(Threading::SINGLE, RuntimeLink::STATIC))
        {
            flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(RuntimeDebugging::OFF, "/MT ", RuntimeDebugging::ON, "/MTd ");
        }

        // Rule register-toolset-really on Line 1852
        SINGLE(RuntimeLink::SHARED, Threading::MULTI);
        // TODO
        // debug-store and pch-source features are being added. don't know where it will be used so holding back

        // TODO Line 1916 PCH Related Variables are not being set

        flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(Optimization::SPEED, "/O2 ", Optimization::SPACE, "/O1 ");
        // TODO:
        // Line 1927 - 1930 skipped because of cpu-type
        if (EVALUATE(Arch::IA64))
        {
            flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(CpuType::ITANIUM, "/G1 ", CpuType::ITANIUM2, "/G2 ");
        }

        // Line 1930
        if (EVALUATE(DebugSymbols::ON))
        {
            flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(DebugStore::OBJECT, "/Z7 ", DebugStore::DATABASE, "/Zi ");
        }
        flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(Optimization::OFF, "/Od ", Inlining::OFF, "/Ob0 ", Inlining::ON,
                                                   "/Ob1 ", Inlining::FULL, "/Ob2 ");
        flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(Warnings::ON, "/W3 ", Warnings::OFF, "/W0 ",
                                                   OR(Warnings::ALL, Warnings::EXTRA, Warnings::PEDANTIC), "/W4 ",
                                                   WarningsAsErrors::ON, "/WX ");
        if (EVALUATE(ExceptionHandling::ON))
        {
            if (EVALUATE(AsyncExceptions::OFF))
            {
                flags.CPP_FLAGS_COMPILE +=
                    GET_FLAG_EVALUATE(ExternCNoThrow::OFF, "/EHs ", ExternCNoThrow::ON, "/EHsc ");
            }
            else if (EVALUATE(AsyncExceptions::ON))
            {
                flags.CPP_FLAGS_COMPILE += GET_FLAG_EVALUATE(ExternCNoThrow::OFF, "/EHa ", ExternCNoThrow::ON, "EHac ");
            }
        }
        flags.CPP_FLAGS_COMPILE += GET_FLAG_EVALUATE(CxxSTD::V_14, "/std:c++14 ", CxxSTD::V_17, "/std:c++17 ",
                                                     CxxSTD::V_20, "/std:c++20 ", CxxSTD::V_LATEST, "/std:c++latest ");
        flags.CPP_FLAGS_COMPILE += GET_FLAG_EVALUATE(RTTI::ON, "/GR ", RTTI::OFF, "/GR- ");
        if (EVALUATE(RuntimeLink::SHARED))
        {
            flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(RuntimeDebugging::OFF, "/MD ", RuntimeDebugging::ON, "/MDd ");
        }
        else if (AND(RuntimeLink::STATIC, Threading::MULTI))
        {
            flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(RuntimeDebugging::OFF, "/MT ", RuntimeDebugging::ON, "/MTd ");
        }

        flags.PDB_CFLAG += GET_FLAG_EVALUATE(AND(DebugSymbols::ON, DebugStore::DATABASE), "/Fd ");

        // TODO// Line 1971
        //  There are variables UNDEFS and FORCE_INCLUDES

        if (EVALUATE(Arch::X86))
        {
            flags.ASMFLAGS_ASM = GET_FLAG_EVALUATE(Warnings::ON, "/W3 ", Warnings::OFF, "/W0 ", Warnings::ALL, "/W4 ",
                                                   WarningsAsErrors::ON, "/WX ");
        }
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
        // TODO:
        //-Wl and --start-group options are needed because gcc command-line is used. But the HMake approach has been to
        // differentiate in compiler and linker
        //  Notes
        //   TODO s
        //   262 Be caustious of this rc-type. It has something to do with Windows resource compiler
        //   which I don't know
        //   TODO: flavor is being assigned based on the -dumpmachine argument to the gcc command. But this
        //    is not yet catered here.

        auto addToBothCOMPILE_FLAGS_and_LINK_FLAGS = [&flags](const string &str) { flags.OPTIONS_COMPILE += str; };
        auto addToBothOPTIONS_COMPILE_CPP_and_OPTIONS_LINK = [&flags](const string &str) {
            flags.OPTIONS_COMPILE_CPP += str;
        };

        // FINDLIBS-SA variable is being updated gcc.link rule.
        string findLibsSA;

        auto isTargetBSD = [&]() { return OR(TargetOS::BSD, TargetOS::FREEBSD, TargetOS::NETBSD, TargetOS::NETBSD); };
        {
            // TargetType is not checked here which is checked in gcc.jam. Maybe better is to manage this through
            // Configuration. -fPIC
            if (!OR(TargetOS::WINDOWS, TargetOS::CYGWIN))
            {
                addToBothCOMPILE_FLAGS_and_LINK_FLAGS("-fPIC ");
            }
        }
        {
            // Handle address-model
            if (AND(TargetOS::AIX, AddressModel::A_32))
            {
                addToBothCOMPILE_FLAGS_and_LINK_FLAGS("-maix32 ");
            }
            if (AND(TargetOS::AIX, AddressModel::A_64))
            {
                addToBothCOMPILE_FLAGS_and_LINK_FLAGS("-maix64 ");
            }
            if (AND(TargetOS::HPUX, AddressModel::A_32))
            {
                addToBothCOMPILE_FLAGS_and_LINK_FLAGS("-milp32 ");
            }
            if (AND(TargetOS::HPUX, AddressModel::A_64))
            {
                addToBothCOMPILE_FLAGS_and_LINK_FLAGS("-milp64 ");
            }
        }
        {
            // Handle threading
            if (EVALUATE(Threading::MULTI))
            {
                if (OR(TargetOS::WINDOWS, TargetOS::CYGWIN, TargetOS::SOLARIS))
                {
                    addToBothCOMPILE_FLAGS_and_LINK_FLAGS("-mthreads ");
                }
                else if (OR(TargetOS::QNX, isTargetBSD()))
                {
                    addToBothCOMPILE_FLAGS_and_LINK_FLAGS("-pthread ");
                }
                else if (EVALUATE(TargetOS::SOLARIS))
                {
                    addToBothCOMPILE_FLAGS_and_LINK_FLAGS("-pthreads ");
                    findLibsSA += "rt";
                }
                else if (!OR(TargetOS::ANDROID, TargetOS::HAIKU, TargetOS::SGI, TargetOS::DARWIN, TargetOS::VXWORKS,
                             TargetOS::IPHONE, TargetOS::APPLETV))
                {
                    addToBothCOMPILE_FLAGS_and_LINK_FLAGS("-pthread ");
                    findLibsSA += "rt";
                }
            }
        }
        {
            auto setCppStdAndDialectCompilerAndLinkerFlags = [&](CxxSTD cxxStdLocal) {
                addToBothOPTIONS_COMPILE_CPP_and_OPTIONS_LINK(cxxStdDialect == CxxSTDDialect::GNU ? "-std=gnu++"
                                                                                                  : "-std=c++");
                CxxSTD temp = cxxStd;
                const_cast<CxxSTD &>(cxxStd) = cxxStdLocal;
                addToBothOPTIONS_COMPILE_CPP_and_OPTIONS_LINK(
                    GET_FLAG_EVALUATE(CxxSTD::V_98, "98 ", CxxSTD::V_03, "03 ", CxxSTD::V_0x, "0x ", CxxSTD::V_11,
                                      "11 ", CxxSTD::V_1y, "1y ", CxxSTD::V_14, "14 ", CxxSTD::V_1z, "1z ",
                                      CxxSTD::V_17, "17 ", CxxSTD::V_2a, "2a ", CxxSTD::V_20, "20 ", CxxSTD::V_2b,
                                      "2b ", CxxSTD::V_23, "23 ", CxxSTD::V_2c, "2c ", CxxSTD::V_26, "26 "));
                const_cast<CxxSTD &>(cxxStd) = temp;
            };

            if (EVALUATE(CxxSTD::V_LATEST))
            {
                // Rule at Line 429
                if (compiler.bTVersion >= Version{10})
                {
                    setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_20);
                }
                else if (compiler.bTVersion >= Version{8})
                {
                    setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_2a);
                }
                else if (compiler.bTVersion >= Version{6})
                {
                    setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_17);
                }
                else if (compiler.bTVersion >= Version{5})
                {
                    setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_1z);
                }
                else if (compiler.bTVersion >= Version{4, 9})
                {
                    setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_14);
                }
                else if (compiler.bTVersion >= Version{4, 8})
                {
                    setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_1y);
                }
                else if (compiler.bTVersion >= Version{4, 7})
                {
                    setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_11);
                }
                else if (compiler.bTVersion >= Version{3, 3})
                {
                    setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_98);
                }
            }
            else
            {
                setCppStdAndDialectCompilerAndLinkerFlags(cxxStd);
            }
        }

        flags.LANG += "-x c++ ";
        // From line 512 to line 625 as no library is using pch or obj

        // General options, link optimizations

        flags.OPTIONS_COMPILE +=
            GET_FLAG_EVALUATE(Optimization::OFF, "-O0 ", Optimization::SPEED, "-O3 ", Optimization::SPACE, "-Os ",
                              Optimization::MINIMAL, "-O1 ", Optimization::DEBUG, "-Og ");

        flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(Inlining::OFF, "-fno-inline ", Inlining::ON, "-Wno-inline ",
                                                   Inlining::FULL, "-finline-functions -Wno-inline ");

        flags.OPTIONS_COMPILE +=
            GET_FLAG_EVALUATE(Warnings::OFF, "-w ", Warnings::ON, "-Wall ", Warnings::ALL, "-Wall ", Warnings::EXTRA,
                              "-Wall -Wextra ", Warnings::PEDANTIC, "-Wall -Wextra -pedantic ");
        flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(WarningsAsErrors::ON, "-Werror ");

        flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(DebugSymbols::ON, "-g ");
        flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(Profiling::ON, "-pg ");

        flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(Visibility::HIDDEN, "-fvisibility=hidden ");
        flags.OPTIONS_COMPILE_CPP += GET_FLAG_EVALUATE(Visibility::HIDDEN, "-fvisibility-inlines-hidden ");
        if (!EVALUATE(TargetOS::DARWIN))
        {
            flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(Visibility::PROTECTED, "-fvisibility=protected ");
        }
        flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(Visibility::GLOBAL, "-fvisibility=default ");

        flags.OPTIONS_COMPILE_CPP += GET_FLAG_EVALUATE(ExceptionHandling::OFF, "-fno-exceptions ");
        flags.OPTIONS_COMPILE_CPP += GET_FLAG_EVALUATE(RTTI::OFF, "-fno-rtti ");

        // Sanitizers
        string sanitizerFlags;
        sanitizerFlags += GET_FLAG_EVALUATE(
            AddressSanitizer::ON, "-fsanitize=address -fno-omit-frame-pointer ", AddressSanitizer::NORECOVER,
            "-fsanitize=address -fno-sanitize-recover=address -fno-omit-frame-pointer ");
        sanitizerFlags +=
            GET_FLAG_EVALUATE(LeakSanitizer::ON, "-fsanitize=leak -fno-omit-frame-pointer ", LeakSanitizer::NORECOVER,
                              "-fsanitize=leak -fno-sanitize-recover=leak -fno-omit-frame-pointer ");
        sanitizerFlags += GET_FLAG_EVALUATE(ThreadSanitizer::ON, "-fsanitize=thread -fno-omit-frame-pointer ",
                                            ThreadSanitizer::NORECOVER,
                                            "-fsanitize=thread -fno-sanitize-recover=thread -fno-omit-frame-pointer ");
        sanitizerFlags += GET_FLAG_EVALUATE(
            UndefinedSanitizer::ON, "-fsanitize=undefined -fno-omit-frame-pointer ", UndefinedSanitizer::NORECOVER,
            "-fsanitize=undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer ");
        sanitizerFlags += GET_FLAG_EVALUATE(Coverage::ON, "--coverage ");

        flags.OPTIONS_COMPILE_CPP += sanitizerFlags;

        if (EVALUATE(TargetOS::VXWORKS))
        {
            flags.DEFINES_COMPILE_CPP += GET_FLAG_EVALUATE(RTTI::OFF, "_NO_RTTI ");
            flags.DEFINES_COMPILE_CPP += GET_FLAG_EVALUATE(ExceptionHandling::OFF, "_NO_EX=1 ");
        }

        // LTO
        if (EVALUATE(LTO::ON))
        {
            flags.OPTIONS_COMPILE +=
                GET_FLAG_EVALUATE(LTOMode::FULL, "-flto ", LTOMode::FAT, "-flto -ffat-lto-objects ");
        }

        // ABI selection
        flags.DEFINES_COMPILE_CPP +=
            GET_FLAG_EVALUATE(StdLib::GNU, "_GLIBCXX_USE_CXX11_ABI=0 ", StdLib::GNU11, "_GLIBCXX_USE_CXX11_ABI=1 ");

        {
            bool noStaticLink = true;
            if (OR(TargetOS::WINDOWS, TargetOS::VMS))
            {
                noStaticLink = false;
            }
            if (noStaticLink && EVALUATE(RuntimeLink::STATIC))
            {
                if (EVALUATE(TargetType::LIBRARY_SHARED))
                {
                    printMessage("WARNING: On gcc, DLLs can not be built with <runtime-link>static\n ");
                }
                else
                {
                    // TODO:  Line 718
                    //  Implement the remaining rule
                }
            }
        }

        string str = GET_FLAG_EVALUATE(
            AND(Arch::X86, InstructionSet::native), "-march=native ", AND(Arch::X86, InstructionSet::i486),
            "-march=i486 ", AND(Arch::X86, InstructionSet::i586), "-march=i586 ", AND(Arch::X86, InstructionSet::i686),
            "-march=i686 ", AND(Arch::X86, InstructionSet::pentium), "-march=pentium ",
            AND(Arch::X86, InstructionSet::pentium_mmx), "-march=pentium-mmx ",
            AND(Arch::X86, InstructionSet::pentiumpro), "-march=pentiumpro ", AND(Arch::X86, InstructionSet::pentium2),
            "-march=pentium2 ", AND(Arch::X86, InstructionSet::pentium3), "-march=pentium3 ",
            AND(Arch::X86, InstructionSet::pentium3m), "-march=pentium3m ", AND(Arch::X86, InstructionSet::pentium_m),
            "-march=pentium-m ", AND(Arch::X86, InstructionSet::pentium4), "-march=pentium4 ",
            AND(Arch::X86, InstructionSet::pentium4m), "-march=pentium4m ", AND(Arch::X86, InstructionSet::prescott),
            "-march=prescott ", AND(Arch::X86, InstructionSet::nocona), "-march=nocona ",
            AND(Arch::X86, InstructionSet::core2), "-march=core2 ", AND(Arch::X86, InstructionSet::conroe),
            "-march=core2 ", AND(Arch::X86, InstructionSet::conroe_xe), "-march=core2 ",
            AND(Arch::X86, InstructionSet::conroe_l), "-march=core2 ", AND(Arch::X86, InstructionSet::allendale),
            "-march=core2 ", AND(Arch::X86, InstructionSet::wolfdale), "-march=core2 -msse4.1 ",
            AND(Arch::X86, InstructionSet::merom), "-march=core2 ", AND(Arch::X86, InstructionSet::merom_xe),
            "-march=core2 ", AND(Arch::X86, InstructionSet::kentsfield), "-march=core2 ",
            AND(Arch::X86, InstructionSet::kentsfield_xe), "-march=core2 ", AND(Arch::X86, InstructionSet::yorksfield),
            "-march=core2 ", AND(Arch::X86, InstructionSet::penryn), "-march=core2 ",
            AND(Arch::X86, InstructionSet::corei7), "-march=corei7 ", AND(Arch::X86, InstructionSet::nehalem),
            "-march=corei7 ", AND(Arch::X86, InstructionSet::corei7_avx), "-march=corei7-avx ",
            AND(Arch::X86, InstructionSet::sandy_bridge), "-march=corei7-avx ",
            AND(Arch::X86, InstructionSet::core_avx_i), "-march=core-avx-i ",
            AND(Arch::X86, InstructionSet::ivy_bridge), "-march=core-avx-i ", AND(Arch::X86, InstructionSet::haswell),
            "-march=core-avx-i -mavx2 -mfma -mbmi -mbmi2 -mlzcnt ", AND(Arch::X86, InstructionSet::broadwell),
            "-march=broadwell ", AND(Arch::X86, InstructionSet::skylake), "-march=skylake ",
            AND(Arch::X86, InstructionSet::skylake_avx512), "-march=skylake-avx512 ",
            AND(Arch::X86, InstructionSet::cannonlake), "-march=skylake-avx512 -mavx512vbmi -mavx512ifma -msha ",
            AND(Arch::X86, InstructionSet::icelake_client), "-march=icelake-client ",
            AND(Arch::X86, InstructionSet::icelake_server), "-march=icelake-server ",
            AND(Arch::X86, InstructionSet::cascadelake), "-march=skylake-avx512 -mavx512vnni ",
            AND(Arch::X86, InstructionSet::cooperlake), "-march=cooperlake ", AND(Arch::X86, InstructionSet::tigerlake),
            "-march=tigerlake ", AND(Arch::X86, InstructionSet::rocketlake), "-march=rocketlake ",
            AND(Arch::X86, InstructionSet::alderlake), "-march=alderlake ",
            AND(Arch::X86, InstructionSet::sapphirerapids), "-march=sapphirerapids ",
            AND(Arch::X86, InstructionSet::k6), "-march=k6 ", AND(Arch::X86, InstructionSet::k6_2), "-march=k6-2 ",
            AND(Arch::X86, InstructionSet::k6_3), "-march=k6-3 ", AND(Arch::X86, InstructionSet::athlon),
            "-march=athlon ", AND(Arch::X86, InstructionSet::athlon_tbird), "-march=athlon-tbird ",
            AND(Arch::X86, InstructionSet::athlon_4), "-march=athlon-4 ", AND(Arch::X86, InstructionSet::athlon_xp),
            "-march=athlon-xp ", AND(Arch::X86, InstructionSet::athlon_mp), "-march=athlon-mp ",
            AND(Arch::X86, InstructionSet::k8), "-march=k8 ", AND(Arch::X86, InstructionSet::opteron),
            "-march=opteron ", AND(Arch::X86, InstructionSet::athlon64), "-march=athlon64 ",
            AND(Arch::X86, InstructionSet::athlon_fx), "-march=athlon-fx ", AND(Arch::X86, InstructionSet::k8_sse3),
            "-march=k8-sse3 ", AND(Arch::X86, InstructionSet::opteron_sse3), "-march=opteron-sse3 ",
            AND(Arch::X86, InstructionSet::athlon64_sse3), "-march=athlon64-sse3 ",
            AND(Arch::X86, InstructionSet::amdfam10), "-march=amdfam10 ", AND(Arch::X86, InstructionSet::barcelona),
            "-march=barcelona ", AND(Arch::X86, InstructionSet::bdver1), "-march=bdver1 ",
            AND(Arch::X86, InstructionSet::bdver2), "-march=bdver2 ", AND(Arch::X86, InstructionSet::bdver3),
            "-march=bdver3 ", AND(Arch::X86, InstructionSet::bdver4), "-march=bdver4 ",
            AND(Arch::X86, InstructionSet::btver1), "-march=btver1 ", AND(Arch::X86, InstructionSet::btver2),
            "-march=btver2 ", AND(Arch::X86, InstructionSet::znver1), "-march=znver1 ",
            AND(Arch::X86, InstructionSet::znver2), "-march=znver2 ", AND(Arch::X86, InstructionSet::znver3),
            "-march=znver3 ", AND(Arch::X86, InstructionSet::winchip_c6), "-march=winchip-c6 ",
            AND(Arch::X86, InstructionSet::winchip2), "-march=winchip2 ", AND(Arch::X86, InstructionSet::c3),
            "-march=c3 ", AND(Arch::X86, InstructionSet::c3_2), "-march=c3-2 ", AND(Arch::X86, InstructionSet::c7),
            "-march=c7 ", AND(Arch::X86, InstructionSet::atom), "-march=atom ",
            AND(Arch::SPARC, InstructionSet::cypress), "-mcpu=cypress ", AND(Arch::SPARC, InstructionSet::v8),
            "-mcpu=v8 ", AND(Arch::SPARC, InstructionSet::supersparc), "-mcpu=supersparc ",
            AND(Arch::SPARC, InstructionSet::sparclite), "-mcpu=sparclite ",
            AND(Arch::SPARC, InstructionSet::hypersparc), "-mcpu=hypersparc ",
            AND(Arch::SPARC, InstructionSet::sparclite86x), "-mcpu=sparclite86x ",
            AND(Arch::SPARC, InstructionSet::f930), "-mcpu=f930 ", AND(Arch::SPARC, InstructionSet::f934),
            "-mcpu=f934 ", AND(Arch::SPARC, InstructionSet::sparclet), "-mcpu=sparclet ",
            AND(Arch::SPARC, InstructionSet::tsc701), "-mcpu=tsc701 ", AND(Arch::SPARC, InstructionSet::v9),
            "-mcpu=v9 ", AND(Arch::SPARC, InstructionSet::ultrasparc), "-mcpu=ultrasparc ",
            AND(Arch::SPARC, InstructionSet::ultrasparc3), "-mcpu=ultrasparc3 ",
            AND(Arch::POWER, InstructionSet::V_403), "-mcpu=403 ", AND(Arch::POWER, InstructionSet::V_505),
            "-mcpu=505 ", AND(Arch::POWER, InstructionSet::V_601), "-mcpu=601 ",
            AND(Arch::POWER, InstructionSet::V_602), "-mcpu=602 ", AND(Arch::POWER, InstructionSet::V_603),
            "-mcpu=603 ", AND(Arch::POWER, InstructionSet::V_603e), "-mcpu=603e ",
            AND(Arch::POWER, InstructionSet::V_604), "-mcpu=604 ", AND(Arch::POWER, InstructionSet::V_604e),
            "-mcpu=604e ", AND(Arch::POWER, InstructionSet::V_620), "-mcpu=620 ",
            AND(Arch::POWER, InstructionSet::V_630), "-mcpu=630 ", AND(Arch::POWER, InstructionSet::V_740),
            "-mcpu=740 ", AND(Arch::POWER, InstructionSet::V_7400), "-mcpu=7400 ",
            AND(Arch::POWER, InstructionSet::V_7450), "-mcpu=7450 ", AND(Arch::POWER, InstructionSet::V_750),
            "-mcpu=750 ", AND(Arch::POWER, InstructionSet::V_801), "-mcpu=801 ",
            AND(Arch::POWER, InstructionSet::V_821), "-mcpu=821 ", AND(Arch::POWER, InstructionSet::V_823),
            "-mcpu=823 ", AND(Arch::POWER, InstructionSet::V_860), "-mcpu=860 ",
            AND(Arch::POWER, InstructionSet::V_970), "-mcpu=970 ", AND(Arch::POWER, InstructionSet::V_8540),
            "-mcpu=8540 ", AND(Arch::POWER, InstructionSet::power), "-mcpu=power ",
            AND(Arch::POWER, InstructionSet::power2), "-mcpu=power2 ", AND(Arch::POWER, InstructionSet::power3),
            "-mcpu=power3 ", AND(Arch::POWER, InstructionSet::power4), "-mcpu=power4 ",
            AND(Arch::POWER, InstructionSet::power5), "-mcpu=power5 ", AND(Arch::POWER, InstructionSet::powerpc),
            "-mcpu=powerpc ", AND(Arch::POWER, InstructionSet::powerpc64), "-mcpu=powerpc64 ",
            AND(Arch::POWER, InstructionSet::rios), "-mcpu=rios ", AND(Arch::POWER, InstructionSet::rios1),
            "-mcpu=rios1 ", AND(Arch::POWER, InstructionSet::rios2), "-mcpu=rios2 ",
            AND(Arch::POWER, InstructionSet::rsc), "-mcpu=rsc ", AND(Arch::POWER, InstructionSet::rs64a), "-mcpu=rs64 ",
            AND(Arch::S390X, InstructionSet::z196), "-march=z196 ", AND(Arch::S390X, InstructionSet::zEC12),
            "-march=zEC12 ", AND(Arch::S390X, InstructionSet::z13), "-march=z13 ",
            AND(Arch::S390X, InstructionSet::z14), "-march=z14 ", AND(Arch::S390X, InstructionSet::z15), "-march=z15 ",
            AND(Arch::ARM, InstructionSet::cortex_a9_p_vfpv3), "-mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard ",
            AND(Arch::ARM, InstructionSet::cortex_a53), "-mcpu=cortex-a53 ", AND(Arch::ARM, InstructionSet::cortex_r5),
            "-mcpu=cortex-r5 ", AND(Arch::ARM, InstructionSet::cortex_r5_p_vfpv3_d16),
            "-mcpu=cortex-r5 -mfpu=vfpv3-d16 -mfloat-abi=hard ");

        flags.OPTIONS += str;
        if (AND(Arch::SPARC, InstructionSet::OFF))
        {
            flags.OPTIONS += "-mcpu=v7 ";
        }
        // 1115
    }
    return flags;
}

void CppSourceTarget::initializeForBuild()
{
    if (!moduleScope)
    {
        moduleScope = this;
        if (!moduleScopeData)
        {
            moduleScopeData = &(moduleScopes.emplace(moduleScope, ModuleScopeData{}).first->second);
        }
    }
    if (!moduleScopeData)
    {
        if (!moduleScope->moduleScopeData)
        {
            moduleScope->moduleScopeData = &(moduleScopes.emplace(moduleScope, ModuleScopeData{}).first->second);
        }
        moduleScopeData = moduleScope->moduleScopeData;
    }
    // Parsing finished
    buildCacheFilesDirPath = getSubDirForTarget() + "Cache_Build_Files/";
}

void CppSourceTarget::preSort(Builder &builder, unsigned short round)
{
    // Try moving following all except round 3 to updateBTarget, so it can be called concurrently as well. Similar in
    // LinkOrArchiveTarget. Then the preSort function is called only once, when configureOrBuild() function is called
    // instead of calling it every round. Also builder can be a global object instead of passing it to all BTarget
    // override functions.
    if (round == 1)
    {
        initializeForBuild();
        readBuildCacheFile(builder);
        // getCompileCommand will be later on called concurrently therefore need to set this before.
        setCompileCommand();
        setSourceCompileCommandPrintFirstHalf();
        parseModuleSourceFiles(builder);
    }
    else if (!round)
    {
        populateSourceNodes();
        resolveRequirePaths();
    }
    else if (round == 3)
    {
        RealBTarget &round3 = getRealBTarget(3);
        for (CPT *cppSourceTarget : requirementDeps)
        {
            round3.addDependency(const_cast<CPT &>(*cppSourceTarget));
        }
        for (CPT *cppSourceTarget : usageRequirementDeps)
        {
            round3.addDependency(const_cast<CPT &>(*cppSourceTarget));
        }

        getRealBTarget(3).fileStatus = FileStatus::NEEDS_UPDATE;
    }
}

void CppSourceTarget::updateBTarget(Builder &, unsigned short round)
{
    if (!round || round == 1)
    {
        if (sourceFileOrSMRuleFileUpdated)
        {
            saveBuildCache(round);
        }
    }
    else if (round == 3)
    {
        populateRequirementAndUsageRequirementDeps();
        addRequirementDepsToBTargetDependencies();
        // Needed to maintain ordering between different includes specification.
        reqIncSizeBeforePopulate = requirementIncludes.size();
        populateTransitiveProperties();
    }
}

void CppSourceTarget::setJson()
{
    Json targetJson;
    targetJson[JConsts::targetType] = compileTargetType;
    targetJson[JConsts::compiler] = compiler;
    targetJson[JConsts::compilerFlags] = requirementCompilerFlags;
    // targetJson[JConsts::linkerFlags] = publicLinkerFlags;
    string str = "SOURCE_";
    // TODO
    //  Remove cached nodes from sourceFileDependencies and moduleSourceFileDependencies
    targetJson[str + JConsts::files] = sourceFileDependencies;
    targetJson[str + JConsts::directories] = regexSourceDirs;
    str = "MODULE_";
    targetJson[str + JConsts::files] = moduleSourceFileDependencies;
    targetJson[str + JConsts::directories] = regexModuleDirs;
    // TODO
    /*    targetJson[JConsts::includeDirectories] = requirementIncludes;
        targetJson[JConsts::huIncludeDirectories] = huIncludes;*/
    // TODO
    //  Add Module Scope
    targetJson[JConsts::compileDefinitions] = requirementCompileDefinitions;
    // TODO
    targetJson[JConsts::variant] = JConsts::project;
    // TODO
    // Should transitive properties be displayed? In that case tranisitve properties be spearated out in
    // addTransitiveProperties Function
    json[0] = std::move(targetJson);
}

void CppSourceTarget::writeJsonFile()
{
    CTarget::writeJsonFile();
}

string CppSourceTarget::getTarjanNodeName() const
{
    return "CppSourceTarget " + getSubDirForTarget();
}

BTarget *CppSourceTarget::getBTarget()
{
    return this;
}

C_Target *CppSourceTarget::get_CAPITarget(BSMode)
{
    auto *c_cppSourceTarget = new C_CppSourceTarget();

    c_cppSourceTarget->parent = reinterpret_cast<C_CTarget *>(CTarget::get_CAPITarget(bsMode)->object);

    c_cppSourceTarget->sourceFilesCount = sourceFileDependencies.size();
    c_cppSourceTarget->sourceFiles = new const char *[sourceFileDependencies.size()];
    unsigned short i = 0;
    for (const SourceNode &sourceNode : sourceFileDependencies)
    {
        c_cppSourceTarget->sourceFiles[i] = sourceNode.node->filePath.c_str();
        ++i;
    }

    c_cppSourceTarget->moduleFilesCount = moduleSourceFileDependencies.size();
    c_cppSourceTarget->moduleFiles = new const char *[moduleSourceFileDependencies.size()];
    i = 0;
    for (const SourceNode &sourceNode : moduleSourceFileDependencies)
    {
        c_cppSourceTarget->sourceFiles[i] = sourceNode.node->filePath.c_str();
        ++i;
    }

    c_cppSourceTarget->includeDirsCount = requirementIncludes.size();
    c_cppSourceTarget->includeDirs = new const char *[requirementIncludes.size()];

    i = 0;
    for (const InclNode &include : requirementIncludes)
    {
        c_cppSourceTarget->includeDirs[i] = include.node->filePath.c_str();
        ++i;
    }

    // setCompileCommand() is called in round 1. Maybe better to run till round 1 in BSMode::IDE and remove following
    setCompileCommand();

    c_cppSourceTarget->compileCommand = compileCommand.c_str();
    auto *compilerPath = new string(compiler.bTPath.generic_string());
    c_cppSourceTarget->compilerPath = compilerPath->c_str();

    auto *c_Target = new C_Target();
    c_Target->type = C_TargetType::C_CPP_TARGET_TYPE;
    c_Target->object = c_cppSourceTarget;
    return c_Target;
}

CppSourceTarget &CppSourceTarget::PUBLIC_COMPILER_FLAGS(const string &compilerFlags)
{
    requirementCompilerFlags += compilerFlags;
    usageRequirementCompilerFlags += compilerFlags;
    return *this;
}

CppSourceTarget &CppSourceTarget::PRIVATE_COMPILER_FLAGS(const string &compilerFlags)
{
    requirementCompilerFlags += compilerFlags;
    return *this;
}

CppSourceTarget &CppSourceTarget::INTERFACE_COMPILER_FLAGS(const string &compilerFlags)
{
    usageRequirementCompilerFlags += compilerFlags;
    return *this;
}

CppSourceTarget &CppSourceTarget::PUBLIC_COMPILE_DEFINITION(const string &cddName, const string &cddValue)
{
    requirementCompileDefinitions.emplace(cddName, cddValue);
    usageRequirementCompileDefinitions.emplace(cddName, cddValue);
    return *this;
}

CppSourceTarget &CppSourceTarget::PRIVATE_COMPILE_DEFINITION(const string &cddName, const string &cddValue)
{
    requirementCompileDefinitions.emplace(cddName, cddValue);
    return *this;
}

CppSourceTarget &CppSourceTarget::INTERFACE_COMPILE_DEFINITION(const string &cddName, const string &cddValue)
{
    usageRequirementCompileDefinitions.emplace(cddName, cddValue);
    return *this;
}

// TODO
// This is being called every time DIRECTORY is being parsed. Also does not differentiates between a simple directory
// and recursive directory
void CppSourceTarget::parseRegexSourceDirs(bool assignToSourceNodes, bool recursive, const SourceDirectory &dir)
{
    if (EVALUATE(TreatModuleAsSource::YES))
    {
        assignToSourceNodes = true;
    }

    auto addNewFile = [&](const auto &k) {
        try
        {
            if (k.is_regular_file() && regex_match(k.path().filename().string(), std::regex(dir.regex)))
            {
                if (assignToSourceNodes)
                {
                    SourceNode &sourceNode = addNodeInSourceFileDependencies(
                        const_cast<Node *>(Node::getNodeFromString(k.path().generic_string(), true)));
                    sourceNode.presentInSource = true;
                }
                else
                {
                    SMFile &smFile = addNodeInModuleSourceFileDependencies(
                        const_cast<Node *>(Node::getNodeFromString(k.path().generic_string(), true)));
                    smFile.presentInSource = true;
                }
            }
        }
        catch (const std::regex_error &e)
        {
            printErrorMessage(fmt::format("regex_error : {}\nError happened while parsing regex {} of target{}\n",
                                          e.what(), dir.regex, getTargetPointer()));
            throw std::exception();
        }
    };

    if (recursive)
    {
        for (const auto &k : recursive_directory_iterator(path(dir.sourceDirectory->filePath)))
        {
            addNewFile(k);
        }
    }
    else
    {
        for (const auto &k : directory_iterator(path(dir.sourceDirectory->filePath)))
        {
            addNewFile(k);
        }
    }
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

    CompilerFlags flags = getCompilerFlags();

    if (compiler.bTFamily == BTFamily::GCC)
    {
        // string str = cSourceTarget == CSourceTarget::YES ? "-xc" : "-xc++";
        compileCommand +=
            flags.LANG + flags.OPTIONS + flags.OPTIONS_COMPILE + flags.OPTIONS_COMPILE_CPP + flags.DEFINES_COMPILE_CPP;
    }
    else if (compiler.bTFamily == BTFamily::MSVC)
    {
        string str = cSourceTarget == CSourceTarget::YES ? "-TC" : "-TP";
        compileCommand += str + " " + flags.CPP_FLAGS_COMPILE_CPP + flags.CPP_FLAGS_COMPILE + flags.OPTIONS_COMPILE +
                          flags.OPTIONS_COMPILE_CPP;
    }

    string translateIncludeFlag = GET_FLAG_EVALUATE(TranslateInclude::YES, "/translateInclude ");
    compileCommand += translateIncludeFlag;

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

    compileCommand += requirementCompilerFlags;

    for (const auto &i : requirementCompileDefinitions)
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

    // Following set is needed because otherwise InclNode propogated from other requirementDeps won't have ordering,
    // because requirementDeps in DS is set. Because of weak ordering this will hurt the caching. Now,
    // requirementIncludes can be made set, but this is not done to maintain specification order for include-dirs

    // I think ideally this should not be support this. A same header-file should not present in more than one
    // header-file.

    unsigned short i = 0;
    auto it = requirementIncludes.begin();
    for (; i < reqIncSizeBeforePopulate; ++i)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(it->node->filePath) + " ");
    }

    set<string> includes;

    for (; it != requirementIncludes.end(); ++it)
    {
        includes.emplace(it->node->filePath);
    }

    for (const string &include : includes)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(include) + " ");
    }
}

void CppSourceTarget::setSourceCompileCommandPrintFirstHalf()
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;

    if (ccpSettings.tool.printLevel != PathPrintLevel::NO)
    {
        sourceCompileCommandPrintFirstHalf +=
            getReducedPath(compiler.bTPath.make_preferred().string(), ccpSettings.tool) + " ";
    }

    if (ccpSettings.infrastructureFlags)
    {
        CompilerFlags flags = getCompilerFlags();
        if (compiler.bTFamily == BTFamily::GCC)
        {
            sourceCompileCommandPrintFirstHalf += flags.LANG + flags.OPTIONS + flags.OPTIONS_COMPILE +
                                                  flags.OPTIONS_COMPILE_CPP + flags.DEFINES_COMPILE_CPP;
        }
        else if (compiler.bTFamily == BTFamily::MSVC)
        {
            sourceCompileCommandPrintFirstHalf += "-TP " + flags.CPP_FLAGS_COMPILE_CPP + flags.CPP_FLAGS_COMPILE +
                                                  flags.OPTIONS_COMPILE + flags.OPTIONS_COMPILE_CPP;
        }
    }

    if (ccpSettings.compilerFlags)
    {
        sourceCompileCommandPrintFirstHalf += requirementCompilerFlags;
    }

    for (const auto &i : requirementCompileDefinitions)
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

    for (const InclNode &include : requirementIncludes)
    {
        if (include.isStandard)
        {
            if (ccpSettings.standardIncludeDirectories.printLevel != PathPrintLevel::NO)
            {
                sourceCompileCommandPrintFirstHalf +=
                    getIncludeFlag() + getReducedPath(include.node->filePath, ccpSettings.standardIncludeDirectories) +
                    " ";
            }
        }
        else
        {
            if (ccpSettings.includeDirectories.printLevel != PathPrintLevel::NO)
            {
                sourceCompileCommandPrintFirstHalf +=
                    getIncludeFlag() + getReducedPath(include.node->filePath, ccpSettings.includeDirectories) + " ";
            }
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

void CppSourceTarget::readBuildCacheFile(Builder &)
{
    if (!std::filesystem::exists(path(buildCacheFilesDirPath)))
    {
        create_directories(buildCacheFilesDirPath);
    }

    if (std::filesystem::exists(path(buildCacheFilesDirPath) / (name + ".cache")))
    {
        ifstream(path(buildCacheFilesDirPath) / (name + ".cache")) >> buildCacheJson;

        for (Json &j : buildCacheJson.at(JConsts::sourceDependencies))
        {
            Node *node = const_cast<Node *>(Node::getNodeFromString(j.at(JConsts::srcFile), true, true));
            if (!node->doesNotExist)
            {
                SourceNode &sourceNode = addNodeInSourceFileDependencies(node);
                sourceNode.presentInCache = true;
                sourceNode.headerFilesJson = std::move(j.at(JConsts::headerDependencies));
                sourceNode.compileCommandJson = j.at(JConsts::compileCommand).get<string>();
            }
        }
        for (Json &j : buildCacheJson.at(JConsts::moduleDependencies))
        {
            Node *node = const_cast<Node *>(Node::getNodeFromString(j.at(JConsts::srcFile), true, true));
            if (!node->doesNotExist)
            {
                SMFile &smFile = addNodeInModuleSourceFileDependencies(node);
                smFile.presentInCache = true;
                smFile.headerFilesJson = std::move(j.at(JConsts::headerDependencies));
                smFile.compileCommandJson = j.at(JConsts::compileCommand).get<string>();
            }
        }

        for (Json &j : buildCacheJson.at(JConsts::headerUnits))
        {
            Node *node = const_cast<Node *>(Node::getNodeFromString(j.at(JConsts::srcFile), true, true));
            if (!node->doesNotExist)
            {
                SMFile &headerUnit = addNodeInHeaderUnits(node);
                headerUnit.presentInCache = true;
                headerUnit.headerFilesJson = std::move(j.at(JConsts::headerDependencies));
                headerUnit.compileCommandJson = j.at(JConsts::compileCommand).get<string>();
            }
        }
    }
}

void CppSourceTarget::resolveRequirePaths()
{
    for (const SMFile &smFileConst : moduleSourceFileDependencies)
    {
        if (smFileConst.presentInSource)
        {
            auto &smFile = const_cast<SMFile &>(smFileConst);
            for (const Json &requireJson : smFile.requiresJson)
            {
                string requireLogicalName = requireJson.at("logical-name").get<string>();
                if (requireLogicalName == smFile.logicalName)
                {
                    printErrorMessageColor(fmt::format("In Scope\n{}\nModule\n{}\n can not depend on itself.\n",
                                                       moduleScope->getSubDirForTarget(), smFile.node->filePath),
                                           settings.pcSettings.toolErrorOutput);
                    throw std::exception();
                }
                if (requireJson.contains("lookup-method"))
                {
                    continue;
                }
                if (auto it = moduleScopeData->requirePaths.find(requireLogicalName);
                    it == moduleScopeData->requirePaths.end())
                {
                    printErrorMessageColor(fmt::format("No File Provides This {}.\n", requireLogicalName),
                                           settings.pcSettings.toolErrorOutput);
                    throw std::exception();
                }
                else
                {
                    SMFile &smFileDep = *(const_cast<SMFile *>(it->second));
                    smFile.getRealBTarget(0).addDependency(smFileDep);
                }
            }
        }
    }
}

void CppSourceTarget::populateSourceNodes()
{
    for (auto it = sourceFileDependencies.begin(); it != sourceFileDependencies.end();)
    {
        if (it->presentInSource)
        {
            auto &sourceNode = const_cast<SourceNode &>(*it);
            sourceNode.setSourceNodeFileStatus(".o", sourceNode.getRealBTarget(0));
            getRealBTarget(0).addDependency(sourceNode);
        }
        ++it;
    }
}

void CppSourceTarget::parseModuleSourceFiles(Builder &)
{
    for (const InclNode *include : huIncludes)
    {
        if (const auto &[pos, Ok] = moduleScopeData->huDirTarget.try_emplace(include, this); !Ok)
        {
            // TODO:
            //  Improve Message
            printErrorMessageColor(
                fmt::format("In ModuleScope {}\nhu-include-directory\n{}\n is being provided by two different "
                            "targets\n{}\n{}\nThis is not allowed "
                            "because HMake can't determine which Header Unit to attach to which target.",
                            moduleScope->getSubDirForTarget(), include->node->filePath, getTargetPointer(),
                            pos->second->getTargetPointer()),
                settings.pcSettings.toolErrorOutput);
            throw std::exception();
        }
    }

    for (auto it = moduleSourceFileDependencies.begin(); it != moduleSourceFileDependencies.end();)
    {
        if (it->presentInSource)
        {
            auto &smFile = const_cast<SMFile &>(*it);
            if (auto [pos, Ok] = moduleScopeData->smFiles.emplace(&smFile); Ok)
            {
                RealBTarget &realBTarget = smFile.getRealBTarget(1);
                smFile.setSourceNodeFileStatus(".smrules", realBTarget);
                if (realBTarget.fileStatus == FileStatus::NEEDS_UPDATE)
                {
                    smFile.generateSMFileInRoundOne = true;
                }
                else
                {
                    realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                }
            }
            else
            {
                printErrorMessageColor(
                    fmt::format("In Module Scope\n{}\nmodule file\n{}\nis being provided by two targets\n{}\n{}\n",
                                moduleScope->getTargetPointer(), smFile.node->filePath, getTargetPointer(),
                                (**pos).target->getTargetPointer()),
                    settings.pcSettings.toolErrorOutput);
                throw std::exception();
            }
            getRealBTarget(0).addDependency(smFile);
        }
        ++it;
    }
}

string CppSourceTarget::getInfrastructureFlags(bool showIncludes)
{
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        string str = GET_FLAG_EVALUATE(TargetType::LIBRARY_OBJECT, "-c", TargetType::PREPROCESS, "-P");
        str += " /nologo ";
        if (showIncludes)
        {
            str += "/showIncludes ";
        }
        return str;
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
        // Will like to use -MD but not using it currently because sometimes it
        // prints 2 header deps in one line and no space in them so no way of
        // knowing whether this is a space in path or 2 different headers. Which
        // then breaks when last_write_time is checked for that path.
        return GET_FLAG_EVALUATE(TargetType::LIBRARY_OBJECT, "-c", TargetType::PREPROCESS, "-E") + " -MMD";
    }
    return "";
}

string CppSourceTarget::getCompileCommandPrintSecondPart(const SourceNode &sourceNode)
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;

    string command;
    if (ccpSettings.infrastructureFlags)
    {
        command += getInfrastructureFlags(true) + " ";
    }
    if (ccpSettings.sourceFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(sourceNode.node->filePath, ccpSettings.sourceFile) + " ";
    }
    if (ccpSettings.infrastructureFlags)
    {
        command += compiler.bTFamily == BTFamily::MSVC ? EVALUATE(TargetType::LIBRARY_OBJECT) ? "/Fo" : "/Fi" : "-o ";
    }
    if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(buildCacheFilesDirPath + path(sourceNode.node->filePath).filename().string() + ".o",
                                  ccpSettings.objectFile) +
                   " ";
    }
    return command;
}

string CppSourceTarget::getCompileCommandPrintSecondPartSMRule(const SMFile &smFile)
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;

    string command;

    if (ccpSettings.sourceFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(smFile.node->filePath, ccpSettings.sourceFile) + " ";
    }
    if (ccpSettings.infrastructureFlags)
    {
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            string translateIncludeFlag = GET_FLAG_EVALUATE(TranslateInclude::YES, "/translateInclude ");
            command += translateIncludeFlag + " /nologo /showIncludes /scanDependencies ";
        }
    }
    if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(buildCacheFilesDirPath + path(smFile.node->filePath).filename().string() + ".smrules",
                                  ccpSettings.objectFile) +
                   " ";
    }

    return command;
}

PostCompile CppSourceTarget::CompileSMFile(SMFile &smFile)
{
    string finalCompileCommand = compileCommand;

    for (const SMFile *smFileLocal : smFile.allSMFileDependenciesRoundZero)
    {
        finalCompileCommand += smFileLocal->getRequireFlag(smFile) + " ";
    }
    finalCompileCommand += getInfrastructureFlags(false) + addQuotes(smFile.node->filePath) + " ";

    finalCompileCommand += smFile.getFlag(buildCacheFilesDirPath + path(smFile.node->filePath).filename().string());

    return PostCompile{*this,
                       compiler,
                       finalCompileCommand,
                       getSourceCompileCommandPrintFirstHalf() + smFile.getModuleCompileCommandPrintLastHalf(),
                       buildCacheFilesDirPath,
                       path(smFile.node->filePath).filename().string(),
                       settings.ccpSettings.outputAndErrorFiles};
}

string CppSourceTarget::getExtension()
{
    return GET_FLAG_EVALUATE(TargetType::PREPROCESS, ".ii", TargetType::LIBRARY_OBJECT, ".o");
}

mutex cppSourceTargetDotCpp_TempMutex;

PostCompile CppSourceTarget::updateSourceNodeBTarget(SourceNode &sourceNode)
{
    // Use atomic_flag instead
    cppSourceTargetDotCpp_TempMutex.lock();
    sourceFileOrSMRuleFileUpdated = true;
    cppSourceTargetDotCpp_TempMutex.unlock();

    string compileFileName = path(sourceNode.node->filePath).filename().string();

    string finalCompileCommand = getCompileCommand() + " ";

    finalCompileCommand += getInfrastructureFlags(true) + " " + addQuotes(sourceNode.node->filePath) + " ";
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        finalCompileCommand += (EVALUATE(TargetType::LIBRARY_OBJECT) ? "/Fo" : "/Fi") +
                               addQuotes(buildCacheFilesDirPath + compileFileName + getExtension()) + " ";
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
        finalCompileCommand += "-o " + addQuotes(buildCacheFilesDirPath + compileFileName + getExtension()) + " ";
    }

    return PostCompile{*this,
                       compiler,
                       finalCompileCommand,
                       getSourceCompileCommandPrintFirstHalf() + getCompileCommandPrintSecondPart(sourceNode),
                       buildCacheFilesDirPath,
                       compileFileName,
                       settings.ccpSettings.outputAndErrorFiles};
}

PostCompile CppSourceTarget::GenerateSMRulesFile(const SMFile &smFile, bool printOnlyOnError)
{
    cppSourceTargetDotCpp_TempMutex.lock();
    sourceFileOrSMRuleFileUpdated = true;
    cppSourceTargetDotCpp_TempMutex.unlock();
    string finalCompileCommand = getCompileCommand() + addQuotes(smFile.node->filePath) + " ";

    if (compiler.bTFamily == BTFamily::MSVC)
    {
        finalCompileCommand +=
            " /nologo /showIncludes /scanDependencies " +
            addQuotes(buildCacheFilesDirPath + path(smFile.node->filePath).filename().string() + ".smrules") + " ";
    }
    else
    {
        printErrorMessageColor("Modules supported only on MSVC\n", settings.pcSettings.toolErrorOutput);
        throw std::exception();
    }

    return printOnlyOnError
               ? PostCompile(*this, compiler, finalCompileCommand, "", buildCacheFilesDirPath,
                             path(smFile.node->filePath).filename().string() + ".smrules",
                             settings.ccpSettings.outputAndErrorFiles)
               : PostCompile(*this, compiler, finalCompileCommand,
                             getSourceCompileCommandPrintFirstHalf() + getCompileCommandPrintSecondPartSMRule(smFile),
                             buildCacheFilesDirPath, path(smFile.node->filePath).filename().string() + ".smrules",
                             settings.ccpSettings.outputAndErrorFiles);
}

void CppSourceTarget::saveBuildCache(bool round)
{
    if (round)
    {
        buildCacheJson[JConsts::moduleDependencies] = moduleSourceFileDependencies;
        buildCacheJson[JConsts::headerUnits] = headerUnits;
        ofstream(path(buildCacheFilesDirPath) / (name + ".cache")) << buildCacheJson.dump(4);
    }
    else
    {
        buildCacheJson[JConsts::sourceDependencies] = sourceFileDependencies;
        buildCacheJson[JConsts::moduleDependencies] = moduleSourceFileDependencies;
        buildCacheJson[JConsts::headerUnits] = headerUnits;
        ofstream(path(buildCacheFilesDirPath) / (name + ".cache")) << buildCacheJson.dump(4);
    }
}

bool operator<(const CppSourceTarget &lhs, const CppSourceTarget &rhs)
{
    return lhs.CTarget::id < rhs.CTarget::id;
}