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

using std::filesystem::create_directories, std::filesystem::recursive_directory_iterator, std::ifstream, std::ofstream,
    std::regex, std::regex_error;

SourceDirectory::SourceDirectory(const string &sourceDirectory_, string regex_)
    : sourceDirectory{Node::getNodeFromString(sourceDirectory_, false)}, regex{std::move(regex_)}
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

void CppSourceTarget::getObjectFiles(vector<ObjectFile *> *objectFiles, LinkOrArchiveTarget *linkOrArchiveTarget) const
{
    for (const SourceNode &objectFile : sourceFileDependencies)
    {
        objectFiles->emplace_back(const_cast<SourceNode *>(&objectFile));
    }
    for (const SMFile &objectFile : moduleSourceFileDependencies)
    {
        objectFiles->emplace_back(const_cast<SMFile *>(&objectFile));
        for (const SMFile *smFile : objectFile.allSMFileDependenciesRoundZero)
        {
            objectFiles->emplace_back(const_cast<SMFile *>(smFile));
        }
    }
    /*    for (const SMFile *objectFile : applicationHeaderUnits)
    {
        objectFiles->emplace_back(const_cast<SMFile *>(objectFile));
        linkRealBTarget.addDependency(const_cast<SMFile &>(*objectFile));
    }*/

    for (const ObjectFileProducer *objectFileTarget : requirementObjectFileTargets)
    {
        objectFileTarget->getObjectFiles(objectFiles, linkOrArchiveTarget);
    }
}

void CppSourceTarget::updateBTarget(unsigned short round, Builder &)
{
    if (!round && selectiveBuild)
    {
        pruneAndSaveBuildCache();
    }
    else if (round == 3)
    {
        populateRequirementAndUsageRequirementDeps();
        addRequirementDepsToBTargetDependencies();
        populateTransitiveProperties();
    }
    getRealBTarget(round).fileStatus = FileStatus::UPDATED;
}

void CppSourceTarget::addRequirementDepsToBTargetDependencies()
{
    // Access to addDependency() function must be synchronized
    std::lock_guard<std::mutex> lk(BTargetNamespace::addDependencyMutex);

    for (CppSourceTarget *cppSourceTarget : requirementDeps)
    {
        getRealBTarget(2).addDependency(const_cast<CppSourceTarget &>(*cppSourceTarget));
    }
}

void CppSourceTarget::populateTransitiveProperties()
{
    for (CppSourceTarget *cppSourceTarget : requirementDeps)
    {
        for (const Node *node : cppSourceTarget->usageRequirementIncludes)
        {
            requirementIncludes.emplace(node);
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

/*void CppSourceTarget::assignDependencyOfLinkOrArchiveTarget(CppSourceTarget *cppSourceTarget) const
{
    if (linkOrArchiveTarget && cppSourceTarget->linkOrArchiveTarget)
    {
        if (linkOrArchiveTarget->linkTargetType == TargetType::LIBRARY_STATIC)
        {
            linkOrArchiveTarget->interfaceLibs.try_emplace(cppSourceTarget->linkOrArchiveTarget);
        }
        else
        {
            linkOrArchiveTarget->privateLibs.try_emplace(cppSourceTarget->linkOrArchiveTarget);
        }
    }
}*/

CppSourceTarget &CppSourceTarget::setModuleScope(CppSourceTarget *moduleScope_)
{
    moduleScope = moduleScope_;
    return *this;
}

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
        //   For Windows NT, we use a different JAMSHELL
        //   Currently, I ain't caring for the propagated properties. These properties are only being
        //   assigned to the parent.
        //   TODO s
        //   262 Be caustious of this rc-type. It has something to do with Windows resource compiler
        //   which I don't know
        //   TODO: flavor is being assigned based on the -dumpmachine argument to the gcc command. But this
        //    is not yet catered here.
        //

        auto addToBothCOMPILE_FLAGS_and_LINK_FLAGS = [&flags](const string &str) { flags.OPTIONS_COMPILE += str; };
        auto addToBothOPTIONS_COMPILE_CPP_and_OPTIONS_LINK = [&flags](const string &str) {
            flags.OPTIONS_COMPILE_CPP += str;
        };

        // FINDLIBS-SA variable is being updated gcc.link rule.
        string findLibsSA;

        auto isTargetBSD = [&]() { return OR(TargetOS::BSD, TargetOS::FREEBSD, TargetOS::NETBSD, TargetOS::NETBSD); };
        {
            // -fPIC
            if (compileTargetType == TargetType::LIBRARY_STATIC || compileTargetType == TargetType::LIBRARY_SHARED)
            {
                if (OR(TargetOS::WINDOWS, TargetOS::CYGWIN))
                {
                }
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

        flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(LocalVisibility::HIDDEN, "-fvisibility=hidden ");
        flags.OPTIONS_COMPILE_CPP += GET_FLAG_EVALUATE(LocalVisibility::HIDDEN, "-fvisibility-inlines-hidden ");
        if (!EVALUATE(TargetOS::DARWIN))
        {
            flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(LocalVisibility::PROTECTED, "-fvisibility=protected ");
        }
        flags.OPTIONS_COMPILE += GET_FLAG_EVALUATE(LocalVisibility::GLOBAL, "-fvisibility=default ");

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
                if (EVALUATE(Link::SHARED))
                {
                    print("WARNING: On gcc, DLLs can not be built with <runtime-link>static\n ");
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
    if (!moduleScopeData && !moduleScope->moduleScopeData)
    {
        moduleScope->moduleScopeData = &(moduleScopes.emplace(moduleScope, ModuleScopeData{}).first->second);
        moduleScopeData = &(moduleScopes.emplace(moduleScope, ModuleScopeData{}).first->second);
    }
    // Parsing finished
    buildCacheFilesDirPath = getSubDirForTarget() + "Cache_Build_Files/";
}

void CppSourceTarget::preSort(Builder &builder, unsigned short round)
{
    if (round == 1)
    {
        initializeForBuild();
        readBuildCacheFile(builder);
        parseModuleSourceFiles(builder);
    }
    else if (!round)
    {
        populateSourceNodes();
        removeUnReferencedHeaderUnits();
        resolveRequirePaths();
    }
    else if (round == 3)
    {
        RealBTarget &round3 = getRealBTarget(3);
        for (CppSourceTarget *cppSourceTarget : requirementDeps)
        {
            round3.addDependency(const_cast<CppSourceTarget &>(*cppSourceTarget));
        }
        for (CppSourceTarget *cppSourceTarget : usageRequirementDeps)
        {
            round3.addDependency(const_cast<CppSourceTarget &>(*cppSourceTarget));
        }

        getRealBTarget(3).fileStatus = FileStatus::NEEDS_UPDATE;
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
    targetJson[str + JConsts::files] = sourceFileDependencies;
    targetJson[str + JConsts::directories] = regexSourceDirs;
    str = "MODULE_";
    targetJson[str + JConsts::files] = moduleSourceFileDependencies;
    targetJson[str + JConsts::directories] = regexModuleDirs;
    targetJson[JConsts::includeDirectories] = requirementIncludes;
    targetJson[JConsts::huIncludeDirectories] = huIncludes;
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

string CppSourceTarget::getTarjanNodeName()
{
    return "CppSourceTarget " + getSubDirForTarget();
}

BTarget *CppSourceTarget::getBTarget()
{
    return this;
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

static void parseRegexSourceDirs(CppSourceTarget &target, bool assignToSourceNodes)
{
    for (const SourceDirectory &sourceDir : assignToSourceNodes ? target.regexSourceDirs : target.regexModuleDirs)
    {
        for (const auto &k : recursive_directory_iterator(path(sourceDir.sourceDirectory->filePath)))
        {
            try
            {
                if (k.is_regular_file() && regex_match(k.path().filename().string(), std::regex(sourceDir.regex)))
                {
                    if (assignToSourceNodes)
                    {
                        SourceNode &sourceNode = target.addNodeInSourceFileDependencies(
                            const_cast<Node *>(Node::getNodeFromString(k.path().generic_string(), true)));
                        sourceNode.presentInSource = true;
                    }
                    else
                    {
                        SMFile &smFile = target.addNodeInModuleSourceFileDependencies(
                            const_cast<Node *>(Node::getNodeFromString(k.path().generic_string(), true)));
                        smFile.presentInSource = true;
                    }
                }
            }
            catch (const std::regex_error &e)
            {
                print(stderr, "regex_error : {}\nError happened while parsing regex {} of target{}\n", e.what(),
                      sourceDir.regex, target.getTargetPointer());
                exit(EXIT_FAILURE);
            }
        }
    }
};

CppSourceTarget &CppSourceTarget::SOURCE_DIRECTORIES(const string &sourceDirectory, const string &regex)
{
    regexSourceDirs.emplace(sourceDirectory, regex);
    parseRegexSourceDirs(*this, true);
    return *this;
}

CppSourceTarget &CppSourceTarget::MODULE_DIRECTORIES(const string &moduleDirectory, const string &regex)
{
    if (EVALUATE(TreatModuleAsSource::YES))
    {
        return SOURCE_DIRECTORIES(moduleDirectory, regex);
    }
    else
    {
        regexModuleDirs.emplace(moduleDirectory, regex);
        parseRegexSourceDirs(*this, false);
        return *this;
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
        compileCommand +=
            flags.LANG + flags.OPTIONS + flags.OPTIONS_COMPILE + flags.OPTIONS_COMPILE_CPP + flags.DEFINES_COMPILE_CPP;
    }
    else if (compiler.bTFamily == BTFamily::MSVC)
    {
        compileCommand += "-TP " + flags.CPP_FLAGS_COMPILE_CPP + flags.CPP_FLAGS_COMPILE + flags.OPTIONS_COMPILE +
                          flags.OPTIONS_COMPILE_CPP;
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

    // Following set is needed because otherwise pointers don't have string-like ordering, so two runs may generate
    // different compile-commands, thus hurting the caching.
    set<string> includes;

    for (const Node *idd : requirementIncludes)
    {
        includes.emplace(idd->filePath);
    }

    for (const Node *stdInclude : standardIncludes)
    {
        includes.emplace(stdInclude->filePath);
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

    for (const Node *idd : requirementIncludes)
    {
        if (ccpSettings.projectIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            sourceCompileCommandPrintFirstHalf +=
                getIncludeFlag() + getReducedPath(idd->filePath, ccpSettings.projectIncludeDirectories) + " ";
        }
    }

    for (const Node *stdInclude : standardIncludes)
    {
        if (ccpSettings.environmentIncludeDirectories.printLevel != PathPrintLevel::NO)
        {
            sourceCompileCommandPrintFirstHalf +=
                getIncludeFlag() + getReducedPath(stdInclude->filePath, ccpSettings.environmentIncludeDirectories) +
                " ";
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
    // getCompileCommand is called concurrently and may cause a data-race, if this hasn't been called before.
    setCompileCommand();
    setSourceCompileCommandPrintFirstHalf();
    if (!std::filesystem::exists(path(buildCacheFilesDirPath)))
    {
        create_directories(buildCacheFilesDirPath);
    }
    path shuCachePath = getSHUSPath();
    if (!exists(shuCachePath))
    {
        create_directories(shuCachePath);
    }

    if (std::filesystem::exists(path(buildCacheFilesDirPath) / (name + ".cache")))
    {
        Json sourceCacheJson;
        ifstream(path(buildCacheFilesDirPath) / (name + ".cache")) >> sourceCacheJson;

        string str = sourceCacheJson.at(JConsts::compileCommand).get<string>();
        auto initializeSourceNodePointer = [](SourceNode &sourceNode, const Json &j, bool smFile) {
            for (const Json &headerFile : j.at(JConsts::headerDependencies))
            {
                Node *node = const_cast<Node *>(Node::getNodeFromString(headerFile, true, true));
                if (node->doesNotExist)
                {
                    if (smFile)
                    {
                        sourceNode.getRealBTarget(1).fileStatus = FileStatus::NEEDS_UPDATE;
                    }
                    else
                    {
                        sourceNode.getRealBTarget(0).fileStatus = FileStatus::NEEDS_UPDATE;
                    }
                }
                else
                {
                    sourceNode.headerDependencies.emplace(node);
                }
            }
            sourceNode.presentInCache = true;
        };

        if (str == compiler.bTPath.generic_string() + " " + getCompileCommand())
        {
            for (const Json &j : sourceCacheJson.at(JConsts::sourceDependencies))
            {
                Node *node = const_cast<Node *>(Node::getNodeFromString(j.at(JConsts::srcFile), true, true));
                if (!node->doesNotExist)
                {
                    initializeSourceNodePointer(addNodeInSourceFileDependencies(node), j, false);
                }
            }
            for (const Json &j : sourceCacheJson.at(JConsts::moduleDependencies))
            {
                Node *node = const_cast<Node *>(Node::getNodeFromString(j.at(JConsts::srcFile), true, true));
                if (!node->doesNotExist)
                {
                    initializeSourceNodePointer(addNodeInModuleSourceFileDependencies(node), j, true);
                }
            }

            for (const Json &j : sourceCacheJson.at(JConsts::headerUnits))
            {
                Node *node = const_cast<Node *>(Node::getNodeFromString(j.at(JConsts::srcFile), true, true));
                if (!node->doesNotExist)
                {
                    SMFile &smFile = addNodeInHeaderUnits(node);
                    smFile.standardHeaderUnit = false;
                    initializeSourceNodePointer(smFile, j, true);
                }
            }
        }
    }
}

void CppSourceTarget::removeUnReferencedHeaderUnits()
{
    for (auto it = applicationHeaderUnits.begin(); it != applicationHeaderUnits.end();)
    {
        if (!(*it)->presentInSource)
        {
            it = applicationHeaderUnits.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void CppSourceTarget::resolveRequirePaths()
{
    for (const SMFile &smFileConst : moduleSourceFileDependencies)
    {
        auto &smFile = const_cast<SMFile &>(smFileConst);
        for (const Json &requireJson : smFileConst.requiresJson)
        {
            string requireLogicalName = requireJson.at("logical-name").get<string>();
            if (requireLogicalName == smFile.logicalName)
            {
                print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                      "In Scope\n{}\nModule\n{}\n can not depend on itself.\n", smFile.node->filePath);
                exit(EXIT_FAILURE);
            }
            if (requireJson.contains("lookup-method"))
            {
                continue;
            }
            if (auto it = moduleScopeData->requirePaths.find(requireLogicalName);
                it == moduleScopeData->requirePaths.end())
            {
                print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                      "No File Provides This {}.\n", requireLogicalName);
                exit(EXIT_FAILURE);
            }
            else
            {
                SMFile &smFileDep = *(const_cast<SMFile *>(it->second));
                smFile.getRealBTarget(0).addDependency(smFileDep);
            }
        }

        // TODO
        /*        if (linkOrArchiveTarget)
                {
                    linkOrArchiveTarget->getRealBTarget(0).addDependency(smFile);
                }*/
        getRealBTarget(0).addDependency(smFile);
    }
}

void CppSourceTarget::populateSourceNodes()
{
    for (auto it = sourceFileDependencies.begin(); it != sourceFileDependencies.end();)
    {
        if (it->presentInSource)
        {
            auto &sourceNode = const_cast<SourceNode &>(*it);
            RealBTarget &realBTarget = sourceNode.getRealBTarget(0);
            if (realBTarget.fileStatus != FileStatus::NEEDS_UPDATE)
            {
                if (!sourceNode.presentInCache)
                {
                    realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                }
                else
                {
                    sourceNode.setSourceNodeFileStatus(".o", realBTarget);
                }
            }
            getRealBTarget(0).addDependency(sourceNode);
            ++it;
        }
        else
        {
            it = sourceFileDependencies.erase(it);
        }
    }
}

void CppSourceTarget::parseModuleSourceFiles(Builder &builder)
{
    for (const Node *idd : huIncludes)
    {
        if (const auto &[pos, Ok] = moduleScopeData->appHUDirTarget.try_emplace(idd, this); !Ok)
        {
            // TODO:
            //  Improve Message
            print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                  "In ModuleScope {}\nhu-include-directory\n{}\n is being provided by two different "
                  "targets\n{}\n{}\nThis is not allowed "
                  "because HMake can't determine which Header Unit to attach to which target.",
                  moduleScope->getSubDirForTarget(), idd->filePath, getTargetPointer(),
                  pos->second->getTargetPointer());
            exit(EXIT_FAILURE);
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
                if (realBTarget.fileStatus != FileStatus::NEEDS_UPDATE)
                {
                    if (!smFile.presentInCache)
                    {
                        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                    }
                    else
                    {
                        smFile.setSourceNodeFileStatus(".smrules", realBTarget);
                    }
                }
                if (realBTarget.fileStatus == FileStatus::NEEDS_UPDATE)
                {
                    smFile.generateSMFileInRoundOne = true;
                }
                else
                {
                    realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                }
                builder.finalBTargets.emplace_back(&smFile);
            }
            else
            {
                print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                      "In Module Scope\n{}\nmodule file\n{}\nis being provided by two targets\n{}\n{}\n",
                      moduleScope->getTargetPointer(), smFile.node->filePath, getTargetPointer(),
                      (**pos).target->getTargetPointer());
                exit(EXIT_FAILURE);
            }
            ++it;
        }
        else
        {
            it = moduleSourceFileDependencies.erase(it);
        }
    }
}

string CppSourceTarget::getInfrastructureFlags()
{
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        return GET_FLAG_EVALUATE(TargetType::LIBRARY_OBJECT, "-c", TargetType::PREPROCESS, "-P") + " /showIncludes ";
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
        command += getInfrastructureFlags() + " ";
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

PostCompile CppSourceTarget::CompileSMFile(SMFile &smFile)
{
    string finalCompileCommand = compileCommand;

    for (const SMFile *smFileLocal : smFile.allSMFileDependenciesRoundZero)
    {
        finalCompileCommand += smFileLocal->getRequireFlag(smFile) + " ";
    }
    finalCompileCommand += getInfrastructureFlags() + addQuotes(smFile.node->filePath) + " ";

    // TODO:
    //  getFlag and getRequireFlags create confusion. Instead of getRequireFlags, only getFlag should be used.
    if (smFile.type == SM_FILE_TYPE::HEADER_UNIT)
    {
        if (smFile.standardHeaderUnit)
        {
            finalCompileCommand += smFile.getFlag(getSHUSPath() + path(smFile.node->filePath).filename().string());
        }
        else
        {
            finalCompileCommand +=
                smFile.getFlag(buildCacheFilesDirPath + path(smFile.node->filePath).filename().string());
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
                       smFile.standardHeaderUnit ? getSHUSPath() : buildCacheFilesDirPath,
                       path(smFile.node->filePath).filename().string(),
                       settings.ccpSettings.outputAndErrorFiles};
}

string CppSourceTarget::getSHUSPath() const
{
    return moduleScope->getSubDirForTarget() + "shus/";
}

string CppSourceTarget::getExtension()
{
    return GET_FLAG_EVALUATE(TargetType::PREPROCESS, ".ii", TargetType::LIBRARY_OBJECT, ".o");
}

PostCompile CppSourceTarget::updateSourceNodeBTarget(SourceNode &sourceNode)
{
    string compileFileName = path(sourceNode.node->filePath).filename().string();

    string finalCompileCommand = getCompileCommand() + " ";

    finalCompileCommand += getInfrastructureFlags() + " " + addQuotes(sourceNode.node->filePath) + " ";
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

PostBasic CppSourceTarget::GenerateSMRulesFile(const SMFile &smFile, bool printOnlyOnError)
{
    string finalCompileCommand = getCompileCommand() + addQuotes(smFile.node->filePath) + " ";

    if (compiler.bTFamily == BTFamily::MSVC)
    {
        string translateIncludeFlag = GET_FLAG_EVALUATE(TranslateInclude::YES, "/translateInclude ");
        finalCompileCommand +=
            translateIncludeFlag + " /scanDependencies " +
            addQuotes(buildCacheFilesDirPath + path(smFile.node->filePath).filename().string() + ".smrules") + " ";
    }
    else
    {
        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
              "Modules supported only on MSVC\n");
        exit(EXIT_FAILURE);
    }

    string outputFilePath = smFile.standardHeaderUnit ? moduleScope->buildCacheFilesDirPath : buildCacheFilesDirPath;
    return printOnlyOnError
               ? PostBasic(compiler, finalCompileCommand, "", outputFilePath,
                           path(smFile.node->filePath).filename().string() + ".smrules",
                           settings.ccpSettings.outputAndErrorFiles, true)
               : PostBasic(compiler, finalCompileCommand,
                           getSourceCompileCommandPrintFirstHalf() + getCompileCommandPrintSecondPart(smFile),
                           outputFilePath, path(smFile.node->filePath).filename().string() + ".smrules",
                           settings.ccpSettings.outputAndErrorFiles, true);
}

void CppSourceTarget::pruneAndSaveBuildCache()
{
    vector<const SourceNode *> sourceFilesLocal;
    vector<const SourceNode *> moduleFilesLocal;
    vector<const SourceNode *> applicationHeaderUnitsLocal;
    for (const SourceNode &sourceNode : sourceFileDependencies)
    {
        if (const_cast<SourceNode &>(sourceNode).getRealBTarget(0).exitStatus == EXIT_SUCCESS)
        {
            sourceFilesLocal.emplace_back(&sourceNode);
        }
    }
    for (const SMFile &smFile : moduleSourceFileDependencies)
    {
        if (const_cast<SMFile &>(smFile).getRealBTarget(0).exitStatus == EXIT_SUCCESS)
        {
            moduleFilesLocal.emplace_back(&smFile);
        }
    }
    for (const SMFile *smFile : applicationHeaderUnits)
    {
        if (const_cast<SMFile *>(smFile)->getRealBTarget(0).exitStatus == EXIT_SUCCESS)
        {
            applicationHeaderUnitsLocal.emplace_back(smFile);
        }
    }
    Json buildCache;
    buildCache[JConsts::compileCommand] = compiler.bTPath.generic_string() + " " + compileCommand;
    buildCache[JConsts::sourceDependencies] = sourceFilesLocal;
    buildCache[JConsts::moduleDependencies] = moduleFilesLocal;
    buildCache[JConsts::headerUnits] = applicationHeaderUnitsLocal;
    ofstream(path(buildCacheFilesDirPath) / (name + ".cache")) << buildCache.dump(4);
}

PLibrary::PLibrary(Configuration &, const path &libraryPath_, TargetType libraryType_)
{
    // TODO
    // Should check if library exists or not
    libraryType = libraryType_;
    if (libraryType != TargetType::PLIBRARY_STATIC || libraryType != TargetType::PLIBRARY_SHARED)
    {
        print(stderr, "PLibrary libraryType TargetType is not one of PLIBRARY_STATIC or PLIBRARY_SHARED \n");
        exit(EXIT_FAILURE);
    }
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