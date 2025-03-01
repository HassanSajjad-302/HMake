
#ifdef USE_HEADER_UNITS
import "Features.hpp";
import "BuildSystemFunctions.hpp";
import "Cache.hpp";
import "CppSourceTarget.hpp";
import "JConsts.hpp";
import "ToolsCache.hpp";
import "nlohmann/json.hpp";
#else
#include "Features.hpp"
#include "BuildSystemFunctions.hpp"
#include "Cache.hpp"
#include "CppSourceTarget.hpp"
#include "JConsts.hpp"
#include "ToolsCache.hpp"
#include "nlohmann/json.hpp"
#endif

#include "CTargetRoundZeroBTarget.hpp"
#include "Configuration.hpp"
using Json = nlohmann::json;

void to_json(Json &j, const Arch &arch)
{
    auto getPStringFromArchitectureEnum = [](const Arch arch) -> string {
        switch (arch)
        {
        case Arch::X86:
            return "X86";
        case Arch::IA64:
            return "IA64";
        case Arch::SPARC:
            return "SPARC";
        case Arch::POWER:
            return "POWER";
        case Arch::LOONGARCH:
            return "LOONGARCH";
        case Arch::MIPS:
            return "MIPS";
        case Arch::MIPS1:
            return "MIPS1";
        case Arch::MIPS2:
            return "MIPS2";
        case Arch::MIPS3:
            return "MIPS3";
        case Arch::MIPS4:
            return "MIPS4";
        case Arch::MIPS32:
            return "MIPS32";
        case Arch::MIPS32R2:
            return "MIPS32R2";
        case Arch::MIPS64:
            return "MIPS64";
        case Arch::PARISC:
            return "PARISC";
        case Arch::ARM:
            return "ARM";
        case Arch::S390X:
            return "S390X";
        case Arch::ARM_P_X86:
            return "ARM_P_X86";
        }
        return "";
    };
    j = getPStringFromArchitectureEnum(arch);
}

void from_json(const Json &j, Arch &arch)
{
    if (j == "X86")
    {
        arch = Arch::X86;
    }
    else if (j == "IA64")
    {
        arch = Arch::IA64;
    }
    else if (j == "SPARC")
    {
        arch = Arch::SPARC;
    }
    else if (j == "POWER")
    {
        arch = Arch::POWER;
    }
    else if (j == "LOONGARCH")
    {
        arch = Arch::LOONGARCH;
    }
    else if (j == "MIPS")
    {
        arch = Arch::MIPS;
    }
    else if (j == "MIPS1")
    {
        arch = Arch::MIPS1;
    }
    else if (j == "MIPS2")
    {
        arch = Arch::MIPS2;
    }
    else if (j == "MIPS3")
    {
        arch = Arch::MIPS3;
    }
    else if (j == "MIPS4")
    {
        arch = Arch::MIPS4;
    }
    else if (j == "MIPS32")
    {
        arch = Arch::MIPS32;
    }
    else if (j == "MIPS32R2")
    {
        arch = Arch::MIPS32R2;
    }
    else if (j == "MIPS64")
    {
        arch = Arch::MIPS64;
    }
    else if (j == "PARISC")
    {
        arch = Arch::PARISC;
    }
    else if (j == "ARM")
    {
        arch = Arch::ARM;
    }
    else if (j == "S390X")
    {
        arch = Arch::S390X;
    }
    else if (j == "ARM_P_X86")
    {
        arch = Arch::ARM_P_X86;
    }
    else
    {
        printErrorMessage("conversion from json string literal to enum class Arch failed\n");
        throw std::exception();
    }
}

void to_json(Json &j, const AddressModel &am)
{
    auto getPStringFromArchitectureEnum = [](const AddressModel am) -> string {
        switch (am)
        {
        case AddressModel::A_16:
            return "A_16";
        case AddressModel::A_32:
            return "A_32";
        case AddressModel::A_64:
            return "A_64";
        case AddressModel::A_32_64:
            return "A_32_64";
        }
        return "";
    };
    j = getPStringFromArchitectureEnum(am);
}

void from_json(const Json &j, AddressModel &am)
{
    if (j == "A_16")
    {
        am = AddressModel::A_16;
    }
    else if (j == "A_32")
    {
        am = AddressModel::A_32;
    }
    else if (j == "A_64")
    {
        am = AddressModel::A_64;
    }
    else if (j == "A_32_64")
    {
        am = AddressModel::A_32_64;
    }
    else
    {
        printErrorMessage("conversion from json string literal to enum class AM failed\n");
        throw std::exception();
    }
}

TemplateDepth::TemplateDepth(const unsigned long long templateDepth_) : templateDepth(templateDepth_)
{
}

Define::Define(string name_, string value_) : name{std::move(name_)}, value{std::move(value_)}
{
}

void to_json(Json &j, const Define &cd)
{
    j[JConsts::name] = cd.name;
    j[JConsts::value] = cd.value;
}

void from_json(const Json &j, Define &cd)
{
    cd.name = j.at(JConsts::name).get<string>();
    cd.value = j.at(JConsts::value).get<string>();
}

void to_json(Json &json, const OS &osLocal)
{
    if (osLocal == OS::NT)
    {
        json = JConsts::windows;
    }
    else if (osLocal == OS::LINUX)
    {
        json = JConsts::linuxUnix;
    }
}

void from_json(const Json &json, OS &osLocal)
{
    if (json == JConsts::windows)
    {
        osLocal = OS::NT;
    }
    else if (json == JConsts::linuxUnix)
    {
        osLocal = OS::LINUX;
    }
}

string getActualNameFromTargetName(const TargetType bTargetType, const OS osLocal, const string &targetName)
{
    if (bTargetType == TargetType::EXECUTABLE)
    {
        return targetName + (osLocal == OS::NT ? ".exe" : "");
    }
    if (bTargetType == TargetType::LIBRARY_STATIC || bTargetType == TargetType::PLIBRARY_STATIC)
    {
        string actualName = osLocal == OS::NT ? "" : "lib";
        actualName += targetName;
        actualName += osLocal == OS::NT ? ".lib" : ".a";
        return actualName;
    }
    if (bTargetType == TargetType::LIBRARY_SHARED || bTargetType == TargetType::PLIBRARY_SHARED)
    {
        string actualName = osLocal == OS::NT ? "" : "lib";
        actualName += targetName;
        actualName += osLocal == OS::NT ? ".dll" : ".so";
        return actualName;
    }
    printErrorMessage("Other Targets Are Not Supported Yet.\n");
    throw std::exception();
}

string getTargetNameFromActualName(const TargetType bTargetType, const OS osLocal, const string &actualName)
{
    if (bTargetType == TargetType::EXECUTABLE)
    {
        return osLocal == OS::NT ? actualName + ".exe" : actualName;
    }
    if (bTargetType == TargetType::LIBRARY_STATIC || bTargetType == TargetType::PLIBRARY_STATIC)
    {
        string libName = actualName;
        // Removes lib from libName.a
        libName = osLocal == OS::NT ? actualName : libName.erase(0, 3);
        // Removes .a from libName.a or .lib from Name.lib
        const unsigned short eraseCount = osLocal == OS::NT ? 4 : 2;
        libName = libName.erase(libName.find('.'), eraseCount);
        return libName;
    }
    if (bTargetType == TargetType::LIBRARY_SHARED || bTargetType == TargetType::PLIBRARY_SHARED)
    {
        string libName = actualName;
        // Removes lib from libName.so
        libName = osLocal == OS::NT ? actualName : libName.erase(0, 3);
        // Removes .so from libName.so or .dll from Name.dll
        const unsigned short eraseCount = osLocal == OS::NT ? 4 : 3;
        libName = libName.erase(libName.find('.'), eraseCount);
        return libName;
    }
    printErrorMessage("Other Targets Are Not Supported Yet.\n");
    throw std::exception();
}

string getSlashedExecutableName(const string &name)
{
    return os == OS::NT ? name + ".exe" : "./" + name;
}

PrebuiltBasicFeatures::PrebuiltBasicFeatures()
{
    if (cache.isLinkerInToolsArray)
    {
        const VSTools &vsTools = toolsCache.vsTools[cache.selectedLinkerArrayIndex];
        if constexpr (bsMode == BSMode::BUILD)
        {
            if (useMiniTarget == UseMiniTarget::YES)
            {
                // Initialized in LinkOrArchiveTarget round 2
                return;
            }
        }
        for (const string &str : vsTools.libraryDirectories)
        {
            Node *node = Node::getNodeFromNonNormalizedPath(str, false);
            bool found = false;
            for (const LibDirNode &libDirNode : requirementLibraryDirectories)
            {
                if (libDirNode.node == node)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                requirementLibraryDirectories.emplace_back(node, true);
            }
        }
    }
}

LinkerFeatures::LinkerFeatures()
{
    // TODO
    // Not Detecting
    addModel = AddressModel::A_64;
    arch = Arch::X86;
    if constexpr (os == OS::NT)
    {
        targetOs = TargetOS::WINDOWS;
    }
    else if constexpr (os == OS::LINUX)
    {
        targetOs = TargetOS::LINUX_;
    }
    setConfigType(configurationType);
    if (cache.isLinkerInToolsArray)
    {
        linker = toolsCache.vsTools[cache.selectedLinkerArrayIndex].linker;
    }
    else
    {
        linker = toolsCache.linkers[cache.selectedLinkerArrayIndex];
    }
    if (cache.isArchiverInToolsArray)
    {
        archiver = toolsCache.vsTools[cache.selectedArchiverArrayIndex].archiver;
    }
    else
    {
        archiver = toolsCache.archivers[cache.selectedArchiverArrayIndex];
    }
}

void LinkerFeatures::setConfigType(const ConfigType configType)
{
    // Value is set according to Comment about variant in variant-feature.jam. But, actually in builtin.jam where
    // variant is set runtime-debugging is not set though. Here it is set, however.
    if (configType == ConfigType::DEBUG)
    {
        debugSymbols = DebugSymbols::ON;
        runtimeDebugging = RuntimeDebugging::ON;
    }
    else if (configType == ConfigType::RELEASE)
    {
        runtimeDebugging = RuntimeDebugging::OFF;
        debugSymbols = DebugSymbols::OFF;
    }
    else if (configType == ConfigType::PROFILE)
    {
        debugSymbols = DebugSymbols::ON;
        profiling = Profiling::ON;
    }
}

void CppCompilerFeatures::initialize(Configuration &config)
{
    // TODO
    // Not Detecting
    addModel = AddressModel::A_64;
    arch = Arch::X86;
    if (targetOs == TargetOS::NONE)
    {
        if constexpr (os == OS::NT)
        {
            targetOs = TargetOS::WINDOWS;
        }
        else if constexpr (os == OS::LINUX)
        {
            targetOs = TargetOS::LINUX_;
        }
    }
    if (configType == ConfigType::NONE)
    {
        setConfigType(ConfigType::RELEASE);
    }
    if (compiler.bTPath == "")
    {
        if (cache.isCompilerInToolsArray)
        {
            if constexpr (os == OS::NT)
            {
                setCompilerFromVSTools(config, toolsCache.vsTools[cache.selectedCompilerArrayIndex]);
            }
            else
            {
                setCompilerFromLinuxTools(config, toolsCache.linuxTools[cache.selectedCompilerArrayIndex]);
            }
        }
        else
        {
            compiler = toolsCache.compilers[cache.selectedCompilerArrayIndex];
        }

        scanner.bTPath = compiler.bTPath.parent_path() / "clang-scan-deps";
    }
}

// Use getNodeFromNormalizedPath instead

void CppCompilerFeatures::setCompilerFromVSTools(Configuration &config, const VSTools &vsTools)
{
    compiler = vsTools.compiler;

    if constexpr (bsMode == BSMode::BUILD)
    {
        if (useMiniTarget == UseMiniTarget::YES)
        {
            // Initialized in CppSourceTarget round 2
            return;
        }
    }
    for (const string &str : vsTools.includeDirectories)
    {
        // TODO
        // This will assign directly to the default target and it will care for the build-time and configure-time.
        actuallyAddInclude(config.cppTargetFeatures.reqIncls, str, true, true);
    }
}

void CppCompilerFeatures::setCompilerFromLinuxTools(Configuration &config, const LinuxTools &linuxTools)
{
    compiler = linuxTools.compiler;
    if constexpr (bsMode == BSMode::BUILD)
    {
        if (useMiniTarget == UseMiniTarget::YES)
        {
            // Initialized in CppSourceTarget round 2
            return;
        }
    }
    for (const string &str : linuxTools.includeDirectories)
    {
        // This will assign directly to the default-target, and it will care for the build-time and configure-time.
        actuallyAddInclude(config.cppTargetFeatures.reqIncls, str, true, true);
    }
}

void CppCompilerFeatures::setConfigType(const ConfigType configType_)
{
    configType = configType_;
    // Value is set according to Comment about variant in variant-feature.jam. But, actually in builtin.jam where
    // variant is set runtime-debugging is not set though. Here it is set, however.
    if (configType == ConfigType::DEBUG)
    {
        optimization = Optimization::OFF;
        inlining = Inlining::OFF;
    }
    else if (configType == ConfigType::RELEASE)
    {
        optimization = Optimization::SPEED;
        inlining = Inlining::FULL;
    }

    // Value is set according to Comment about variant in variant-feature.jam. But, actually in builtin.jam where
    // variant is set runtime-debugging is not set though. Here it is set, however.
    if (configType == ConfigType::DEBUG)
    {
        debugSymbols = DebugSymbols::ON;
        runtimeDebugging = RuntimeDebugging::ON;
    }
    else if (configType == ConfigType::RELEASE)
    {
        runtimeDebugging = RuntimeDebugging::OFF;
        debugSymbols = DebugSymbols::OFF;
    }
    else if (configType == ConfigType::PROFILE)
    {
        debugSymbols = DebugSymbols::ON;
        profiling = Profiling::ON;
    }
}

// For some features the resultant object-file is same these are termed incidental. Change these does not result in
// recompilation. Skip these in compiler-command that is cached.
CompilerFlags CppCompilerFeatures::getCompilerFlags() const
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
        string defaultAssembler = evaluate(Arch::IA64) ? "ias " : "";
        if (evaluate(Arch::X86))
        {
            defaultAssembler += GET_FLAG_evaluate(AddressModel::A_64, "ml64 ", AddressModel::A_32, "ml -coff ");
        }
        else if (evaluate(Arch::ARM))
        {
            defaultAssembler += GET_FLAG_evaluate(AddressModel::A_64, "armasm64 ", AddressModel::A_32, "armasm ");
        }
        string assemblerFlags = GET_FLAG_evaluate(OR(Arch::X86, Arch::IA64), "-c -Zp4 -Cp -Cx ");
        string assemblerOutputFlag = GET_FLAG_evaluate(OR(Arch::X86, Arch::IA64), "-Fo ", Arch::ARM, "-o ");
        // Line 1618

        // Line 1650
        flags.DOT_CC_COMPILE += "/Zm800 -nologo ";
        flags.DOT_ASM_COMPILE += defaultAssembler + assemblerFlags + "-nologo ";
        flags.DOT_ASM_OUTPUT_COMPILE += assemblerOutputFlag;
        flags.DOT_LD_ARCHIVE += "lib /NOLOGO ";

        // Line 1670
        flags.OPTIONS_COMPILE += GET_FLAG_evaluate(LTO::ON, "/GL ");
        // End-Line 1682

        // Function completed. Jumping to rule configure-version-specific.
        // Line 444
        // Only flags effecting latest MSVC tools (14.3) are supported.

        flags.OPTIONS_COMPILE += "/Zc:forScope /Zc:wchar_t ";
        flags.CPP_FLAGS_COMPILE_CPP = "/wd4675 ";
        flags.OPTIONS_COMPILE += GET_FLAG_evaluate(Warnings::OFF, "/wd4996 ");
        flags.OPTIONS_COMPILE += "/Zc:inline ";
        flags.OPTIONS_COMPILE += GET_FLAG_evaluate(OR(Optimization::SPEED, Optimization::SPACE), "/Gw ");
        flags.CPP_FLAGS_COMPILE_CPP += "/Zc:throwingNew ";

        // Line 492
        flags.OPTIONS_COMPILE += GET_FLAG_evaluate(AddressSanitizer::ON, "/fsanitize=address /FS ");

        if (evaluate(AddressModel::A_64))
        {
            // The various 64 bit runtime asan support libraries and related flags.
            string FINDLIBS_SA_LINK =
                GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::SHARED),
                                  "clang_rt.asan_dynamic-x86_64 clang_rt.asan_dynamic_runtime_thunk-x86_64 ");
            /*FINDLIBS_SA_LINK +=
                GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::STATIC, TargetType::EXECUTABLE),
                                  "clang_rt.asan-x86_64 clang_rt.asan_cxx-x86_64 ");*/
            string FINDLIBS_SA_LINK_DLL =
                GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::STATIC), "clang_rt.asan_dll_thunk-x86_64 ");
            string LINKFLAGS_LINK_DLL = GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::STATIC),
                                                          R"(/wholearchive\:"clang_rt.asan_dll_thunk-x86_64.lib ")");
        }
        else if (evaluate(AddressModel::A_32))
        {
            // The various 32 bit runtime asan support libraries and related flags.

            string FINDLIBS_SA_LINK =
                GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::SHARED),
                                  "clang_rt.asan_dynamic-i386 clang_rt.asan_dynamic_runtime_thunk-i386 ");
            /*FINDLIBS_SA_LINK +=
                GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::STATIC, TargetType::EXECUTABLE),
                                  "clang_rt.asan-i386 clang_rt.asan_cxx-i386 ");*/
            string FINDLIBS_SA_LINK_DLL =
                GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::STATIC), "clang_rt.asan_dll_thunk-i386 ");
            string LINKFLAGS_LINK_DLL = GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::STATIC),
                                                          R"(/wholearchive\:"clang_rt.asan_dll_thunk-i386.lib ")");
        }

        // Line 586
        if (AND(Arch::X86, AddressModel::A_32))
        {
            flags.OPTIONS_COMPILE += "/favor:blend ";
            flags.OPTIONS_COMPILE +=
                GET_FLAG_evaluate(CpuType::EM64T, "/favor:EM64T ", CpuType::AMD64, "/favor:AMD64 ");
        }
        if (AND(Threading::SINGLE, RuntimeLink::STATIC))
        {
            flags.OPTIONS_COMPILE += GET_FLAG_evaluate(RuntimeDebugging::OFF, "/MT ", RuntimeDebugging::ON, "/MTd ");
        }

        // Rule register-toolset-really on Line 1852
        if (evaluate(RuntimeLink::SHARED) && !evaluate(Threading::MULTI))
        {
            throw std::exception("With RuntimeLink::SHARED, Threading::MULTI must be used");
        }
        // TODO
        // debug-store and pch-source features are being added. don't know where it will be used so holding back

        // TODO Line 1916 PCH Related Variables are not being set

        flags.OPTIONS_COMPILE += GET_FLAG_evaluate(Optimization::SPEED, "/O2 ", Optimization::SPACE, "/O1 ");
        // TODO:
        // Line 1927 - 1930 skipped because of cpu-type
        if (evaluate(Arch::IA64))
        {
            flags.OPTIONS_COMPILE += GET_FLAG_evaluate(CpuType::ITANIUM, "/G1 ", CpuType::ITANIUM2, "/G2 ");
        }

        // Line 1930
        if (evaluate(DebugSymbols::ON))
        {
            flags.OPTIONS_COMPILE += GET_FLAG_evaluate(DebugStore::OBJECT, "/Z7 ", DebugStore::DATABASE, "/Zi ");
        }
        flags.OPTIONS_COMPILE += GET_FLAG_evaluate(Optimization::OFF, "/Od ", Inlining::OFF, "/Ob0 ", Inlining::ON,
                                                   "/Ob1 ", Inlining::FULL, "/Ob2 ");
        flags.OPTIONS_COMPILE += GET_FLAG_evaluate(Warnings::ON, "/W3 ", Warnings::OFF, "/W0 ",
                                                   OR(Warnings::ALL, Warnings::EXTRA, Warnings::PEDANTIC), "/W4 ",
                                                   WarningsAsErrors::ON, "/WX ");
        if (evaluate(ExceptionHandling::ON))
        {
            if (evaluate(AsyncExceptions::OFF))
            {
                flags.CPP_FLAGS_COMPILE +=
                    GET_FLAG_evaluate(ExternCNoThrow::OFF, "/EHs ", ExternCNoThrow::ON, "/EHsc ");
            }
            else if (evaluate(AsyncExceptions::ON))
            {
                flags.CPP_FLAGS_COMPILE += GET_FLAG_evaluate(ExternCNoThrow::OFF, "/EHa ", ExternCNoThrow::ON, "EHac ");
            }
        }
        flags.CPP_FLAGS_COMPILE += GET_FLAG_evaluate(CxxSTD::V_14, "/std:c++14 ", CxxSTD::V_17, "/std:c++17 ",
                                                     CxxSTD::V_20, "/std:c++20 ", CxxSTD::V_LATEST, "/std:c++latest ");
        flags.CPP_FLAGS_COMPILE += GET_FLAG_evaluate(RTTI::ON, "/GR ", RTTI::OFF, "/GR- ");
        if (evaluate(RuntimeLink::SHARED))
        {
            flags.OPTIONS_COMPILE += GET_FLAG_evaluate(RuntimeDebugging::OFF, "/MD ", RuntimeDebugging::ON, "/MDd ");
        }
        else if (AND(RuntimeLink::STATIC, Threading::MULTI))
        {
            flags.OPTIONS_COMPILE += GET_FLAG_evaluate(RuntimeDebugging::OFF, "/MT ", RuntimeDebugging::ON, "/MTd ");
        }

        flags.PDB_CFLAG += GET_FLAG_evaluate(AND(DebugSymbols::ON, DebugStore::DATABASE), "/Fd ");

        // TODO// Line 1971
        //  There are variables UNDEFS and FORCE_INCLUDES

        if (evaluate(Arch::X86))
        {
            flags.ASMFLAGS_ASM = GET_FLAG_evaluate(Warnings::ON, "/W3 ", Warnings::OFF, "/W0 ", Warnings::ALL, "/W4 ",
                                                   WarningsAsErrors::ON, "/WX ");
        }
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
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
            if (evaluate(Threading::MULTI))
            {
                if (OR(TargetOS::WINDOWS, TargetOS::CYGWIN, TargetOS::SOLARIS))
                {
                    addToBothCOMPILE_FLAGS_and_LINK_FLAGS("-mthreads ");
                }
                else if (OR(TargetOS::QNX, isTargetBSD()))
                {
                    addToBothCOMPILE_FLAGS_and_LINK_FLAGS("-pthread ");
                }
                else if (evaluate(TargetOS::SOLARIS))
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
            auto setCppStdAndDialectCompilerAndLinkerFlags = [&](const CxxSTD cxxStdLocal) {
                addToBothOPTIONS_COMPILE_CPP_and_OPTIONS_LINK(cxxStdDialect == CxxSTDDialect::GNU ? "-std=gnu++"
                                                                                                  : "-std=c++");
                const CxxSTD temp = cxxStd;
                const_cast<CxxSTD &>(cxxStd) = cxxStdLocal;
                addToBothOPTIONS_COMPILE_CPP_and_OPTIONS_LINK(
                    GET_FLAG_evaluate(CxxSTD::V_98, "98 ", CxxSTD::V_03, "03 ", CxxSTD::V_0x, "0x ", CxxSTD::V_11,
                                      "11 ", CxxSTD::V_1y, "1y ", CxxSTD::V_14, "14 ", CxxSTD::V_1z, "1z ",
                                      CxxSTD::V_17, "17 ", CxxSTD::V_2a, "2a ", CxxSTD::V_20, "20 ", CxxSTD::V_2b,
                                      "2b ", CxxSTD::V_23, "23 ", CxxSTD::V_2c, "2c ", CxxSTD::V_26, "26 "));
                const_cast<CxxSTD &>(cxxStd) = temp;
            };

            if (evaluate(CxxSTD::V_LATEST))
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
            GET_FLAG_evaluate(Optimization::OFF, "-O0 ", Optimization::SPEED, "-O3 ", Optimization::SPACE, "-Os ",
                              Optimization::MINIMAL, "-O1 ", Optimization::DEBUG, "-Og ");

        flags.OPTIONS_COMPILE += GET_FLAG_evaluate(Inlining::OFF, "-fno-inline ", Inlining::ON, "-Wno-inline ",
                                                   Inlining::FULL, "-finline-functions -Wno-inline ");

        flags.OPTIONS_COMPILE +=
            GET_FLAG_evaluate(Warnings::OFF, "-w ", Warnings::ON, "-Wall ", Warnings::ALL, "-Wall ", Warnings::EXTRA,
                              "-Wall -Wextra ", Warnings::PEDANTIC, "-Wall -Wextra -pedantic ");
        flags.OPTIONS_COMPILE += GET_FLAG_evaluate(WarningsAsErrors::ON, "-Werror ");

        flags.OPTIONS_COMPILE += GET_FLAG_evaluate(DebugSymbols::ON, "-g ");
        flags.OPTIONS_COMPILE += GET_FLAG_evaluate(Profiling::ON, "-pg ");

        flags.OPTIONS_COMPILE += GET_FLAG_evaluate(Visibility::HIDDEN, "-fvisibility=hidden ");
        flags.OPTIONS_COMPILE_CPP += GET_FLAG_evaluate(Visibility::HIDDEN, "-fvisibility-inlines-hidden ");
        if (!evaluate(TargetOS::DARWIN))
        {
            flags.OPTIONS_COMPILE += GET_FLAG_evaluate(Visibility::PROTECTED, "-fvisibility=protected ");
        }
        flags.OPTIONS_COMPILE += GET_FLAG_evaluate(Visibility::GLOBAL, "-fvisibility=default ");

        flags.OPTIONS_COMPILE_CPP += GET_FLAG_evaluate(ExceptionHandling::OFF, "-fno-exceptions ");
        flags.OPTIONS_COMPILE_CPP += GET_FLAG_evaluate(RTTI::OFF, "-fno-rtti ");

        // Sanitizers
        string sanitizerFlags;
        sanitizerFlags += GET_FLAG_evaluate(
            AddressSanitizer::ON, "-fsanitize=address -fno-omit-frame-pointer ", AddressSanitizer::NORECOVER,
            "-fsanitize=address -fno-sanitize-recover=address -fno-omit-frame-pointer ");
        sanitizerFlags +=
            GET_FLAG_evaluate(LeakSanitizer::ON, "-fsanitize=leak -fno-omit-frame-pointer ", LeakSanitizer::NORECOVER,
                              "-fsanitize=leak -fno-sanitize-recover=leak -fno-omit-frame-pointer ");
        sanitizerFlags += GET_FLAG_evaluate(ThreadSanitizer::ON, "-fsanitize=thread -fno-omit-frame-pointer ",
                                            ThreadSanitizer::NORECOVER,
                                            "-fsanitize=thread -fno-sanitize-recover=thread -fno-omit-frame-pointer ");
        sanitizerFlags += GET_FLAG_evaluate(
            UndefinedSanitizer::ON, "-fsanitize=undefined -fno-omit-frame-pointer ", UndefinedSanitizer::NORECOVER,
            "-fsanitize=undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer ");
        sanitizerFlags += GET_FLAG_evaluate(Coverage::ON, "--coverage ");

        flags.OPTIONS_COMPILE_CPP += sanitizerFlags;

        if (evaluate(TargetOS::VXWORKS))
        {
            flags.DEFINES_COMPILE_CPP += GET_FLAG_evaluate(RTTI::OFF, "_NO_RTTI ");
            flags.DEFINES_COMPILE_CPP += GET_FLAG_evaluate(ExceptionHandling::OFF, "_NO_EX=1 ");
        }

        // LTO
        if (evaluate(LTO::ON))
        {
            flags.OPTIONS_COMPILE +=
                GET_FLAG_evaluate(LTOMode::FULL, "-flto ", LTOMode::FAT, "-flto -ffat-lto-objects ");
        }

        // ABI selection
        flags.DEFINES_COMPILE_CPP +=
            GET_FLAG_evaluate(StdLib::GNU, "_GLIBCXX_USE_CXX11_ABI=0 ", StdLib::GNU11, "_GLIBCXX_USE_CXX11_ABI=1 ");

        {
            bool noStaticLink = true;
            if (OR(TargetOS::WINDOWS, TargetOS::VMS))
            {
                noStaticLink = false;
            }
            /*if (noStaticLink && evaluate(RuntimeLink::STATIC))
            {
                if (evaluate(TargetType::LIBRARY_SHARED))
                {
                    printMessage("WARNING: On gcc, DLLs can not be built with <runtime-link>static\n ");
                }
                else
                {
                    // TODO:  Line 718
                    //  Implement the remaining rule
                }
            }*/
        }

        string str = GET_FLAG_evaluate(
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

void CppCompilerFeatures::setCpuType()
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

bool CppCompilerFeatures::isCpuTypeG7()
{
    return OR(InstructionSet::pentium4, InstructionSet::pentium4m, InstructionSet::athlon, InstructionSet::athlon_tbird,
              InstructionSet::athlon_4, InstructionSet::athlon_xp, InstructionSet::athlon_mp, CpuType::EM64T,
              CpuType::AMD64);
}
