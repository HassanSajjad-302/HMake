
#ifdef USE_HEADER_UNITS
import "LinkOrArchiveTarget.hpp";
import "BuildSystemFunctions.hpp";
import "Cache.hpp";
import "CppSourceTarget.hpp";
import "Utilities.hpp";
import <filesystem>;
import <fstream>;
import <utility>;
#else
#include "LinkOrArchiveTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Cache.hpp"
#include "CppSourceTarget.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <fstream>
#include <utility>
#endif

using std::ofstream, std::filesystem::create_directories, std::ifstream;

PrebuiltLinkOrArchiveTarget::PrebuiltLinkOrArchiveTarget(const string &name, const string &directory,
                                                         TargetType linkTargetType_)
    : linkTargetType(linkTargetType_), outputDirectory(Node::getFinalNodePathFromString(directory).string()),
      outputName(name), actualOutputName(getActualNameFromTargetName(linkTargetType_, os, name))
{
}

void PrebuiltLinkOrArchiveTarget::preSort(Builder &builder, unsigned short round)
{
    if (round == 3)
    {

        RealBTarget &round3 = getRealBTarget(3);
        for (PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget : requirementDeps)
        {
            round3.addDependency(const_cast<PrebuiltLinkOrArchiveTarget &>(*prebuiltLinkOrArchiveTarget));
        }
        for (PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget : usageRequirementDeps)
        {
            round3.addDependency(const_cast<PrebuiltLinkOrArchiveTarget &>(*prebuiltLinkOrArchiveTarget));
        }
        getRealBTarget(3).fileStatus = FileStatus::NEEDS_UPDATE;
    }
}

void PrebuiltLinkOrArchiveTarget::updateBTarget(Builder &builder, unsigned short round)
{
    if (round == 3)
    {
        populateRequirementAndUsageRequirementDeps();
        addRequirementDepsToBTargetDependencies();
    }
}

void PrebuiltLinkOrArchiveTarget::addRequirementDepsToBTargetDependencies()
{
    // Access to addDependency() function must be synchronized because set::emplace is not thread-safe
    std::lock_guard<std::mutex> lk(BTargetNamespace::addDependencyMutex);
    RealBTarget &round0 = getRealBTarget(0);
    RealBTarget &round2 = getRealBTarget(2);
    for (PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget : requirementDeps)
    {
        round0.addDependency(const_cast<PrebuiltLinkOrArchiveTarget &>(*prebuiltLinkOrArchiveTarget));
        round2.addDependency(const_cast<PrebuiltLinkOrArchiveTarget &>(*prebuiltLinkOrArchiveTarget));
    }
}

string PrebuiltLinkOrArchiveTarget::getActualOutputPath()
{
    return outputDirectory + actualOutputName;
}

void to_json(Json &json, const PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget)
{
    json = prebuiltLinkOrArchiveTarget.getTarjanNodeName();
}

LinkOrArchiveTarget::LinkOrArchiveTarget(string name_, TargetType targetType)
    : CTarget(std::move(name_)), PrebuiltLinkOrArchiveTarget(name, getSubDirForTarget(), targetType)
{
    linkTargetType = targetType;
}

LinkOrArchiveTarget::LinkOrArchiveTarget(string name_, TargetType targetType, CTarget &other, bool hasFile)
    : CTarget(std::move(name_), other, hasFile), PrebuiltLinkOrArchiveTarget(name, getSubDirForTarget(), targetType)
{
    linkTargetType = targetType;
}

void LinkOrArchiveTarget::setLinkerFromVSTools(struct VSTools &vsTools)
{
    linker = vsTools.linker;
}

void LinkOrArchiveTarget::initializeForBuild()
{
    buildCacheFilesDirPath = getSubDirForTarget() + "Cache_Build_Files/";
}

void LinkOrArchiveTarget::populateObjectFiles()
{
    for (const ObjectFileProducer *objectFileProducer : objectFileProducers)
    {
        objectFileProducer->getObjectFiles(&objectFiles, this);
    }
}

void LinkOrArchiveTarget::preSort(Builder &builder, unsigned short round)
{
    if (!round)
    {
        initializeForBuild();
        for (ObjectFileProducer *objectFileProducer : objectFileProducers)
        {
            objectFileProducer->addDependencyOnObjectFileProducers(this);
        }
        checkForPreBuiltAndCacheDir(builder);
    }
    else if (round == 3)
    {
        PrebuiltLinkOrArchiveTarget::preSort(builder, round);
    }
}

void LinkOrArchiveTarget::duringSort(Builder &, unsigned short round, unsigned int)
{
    if (!round)
    {
        populateObjectFiles();
        RealBTarget &realBTarget = getRealBTarget(round);
        if (realBTarget.fileStatus != FileStatus::NEEDS_UPDATE)
        {
            path outputPath = path(getActualOutputPath());
            if (!std::filesystem::exists(outputPath))
            {
                realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                return;
            }
            for (PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget : requirementDeps)
            {
                path depOutputPath = path(prebuiltLinkOrArchiveTarget->getActualOutputPath());
                if (Node::getNodeFromString(depOutputPath.generic_string(), true)->getLastUpdateTime() >
                    Node::getNodeFromString(outputPath.generic_string(), true)->getLastUpdateTime())
                {
                    realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                    return;
                }
            }
            for (ObjectFile *objectFile : objectFiles)
            {
                if (!cachedObjectFiles.contains(objectFile->getObjectFileOutputFilePath()))
                {
                    realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                    return;
                }
                if (Node::getNodeFromString(objectFile->getObjectFileOutputFilePath(), true)->getLastUpdateTime() >
                    Node::getNodeFromString(outputPath.generic_string(), true)->getLastUpdateTime())
                {
                    realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                    return;
                }
            }
        }
    }
}

void LinkOrArchiveTarget::updateBTarget(Builder &builder, unsigned short round)
{
    RealBTarget &realBTarget = getRealBTarget(round);
    if (!round && BTarget::selectiveBuild && realBTarget.exitStatus == EXIT_SUCCESS)
    {

        shared_ptr<PostBasic> postBasicLinkOrArchive;
        if (linkTargetType == TargetType::LIBRARY_STATIC)
        {
            postBasicLinkOrArchive = std::make_shared<PostBasic>(Archive());
        }
        else if (linkTargetType == TargetType::EXECUTABLE || linkTargetType == TargetType::LIBRARY_SHARED)
        {
            postBasicLinkOrArchive = std::make_shared<PostBasic>(Link());
        }
        realBTarget.exitStatus = postBasicLinkOrArchive->exitStatus;
        if (postBasicLinkOrArchive->exitStatus == EXIT_SUCCESS)
        {
            Json cacheFileJson;
            cacheFileJson[JConsts::linkCommand] = linker.bTPath.generic_string() + " " + getLinkOrArchiveCommand(true);
            vector<string> cachedObjectFilesVector;
            for (ObjectFile *objectFile : objectFiles)
            {
                cachedObjectFilesVector.emplace_back(objectFile->getObjectFileOutputFilePath());
            }
            cacheFileJson[JConsts::objectFiles] = std::move(cachedObjectFilesVector);
            ofstream(path(buildCacheFilesDirPath) / (name + ".cache")) << cacheFileJson.dump(4);
        }

        std::lock_guard<std::mutex> lk(printMutex);
        if (linkTargetType == TargetType::LIBRARY_STATIC)
        {
            postBasicLinkOrArchive->executePrintRoutine(settings.pcSettings.archiveCommandColor, false);
        }
        else if (linkTargetType == TargetType::EXECUTABLE || linkTargetType == TargetType::LIBRARY_SHARED)
        {
            postBasicLinkOrArchive->executePrintRoutine(settings.pcSettings.linkCommandColor, false);
        }
        fflush(stdout);
    }
    else if (round == 3)
    {
        PrebuiltLinkOrArchiveTarget::updateBTarget(builder, 3);
        addRequirementDepsToBTargetDependencies();
        for (PrebuiltLinkOrArchiveTarget *linkOrArchiveTarget : requirementDeps)
        {
            requirementLinkerFlags += linkOrArchiveTarget->usageRequirementLinkerFlags;
        }
    }
    realBTarget.fileStatus = FileStatus::UPDATED;
}

LinkerFlags LinkOrArchiveTarget::getLinkerFlags()
{
    LinkerFlags flags;
    if (linker.bTFamily == BTFamily::MSVC)
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
        string defaultAssembler = EVALUATE(Arch::IA64) ? "ias" : "";
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

        flags.DOT_LD_LINK += "/NOLOGO /INCREMENTAL:NO";
        flags.DOT_LD_ARCHIVE += "lib /NOLOGO";

        flags.LINKFLAGS_LINK += GET_FLAG_EVALUATE(LTO::ON, "/LTCG ");
        // End-Line 1682

        // Function completed. Jumping to rule configure-version-specific.
        // Line 444
        // Only flags effecting latest MSVC tools (14.3) are supported.

        string CPP_FLAGS_COMPILE_CPP = "/wd4675";
        CPP_FLAGS_COMPILE_CPP += "/Zc:throwingNew";

        // Line 492
        flags.LINKFLAGS_LINK += GET_FLAG_EVALUATE(AddressSanitizer::ON, "-incremental:no ");

        /*        if (EVALUATE(AddressModel::A_64))
                {
                    // The various 64 bit runtime asan support libraries and related flags.
                    flags.FINDLIBS_SA_LINK =
                        GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::SHARED),
                                          "clang_rt.asan_dynamic-x86_64 clang_rt.asan_dynamic_runtime_thunk-x86_64 ");
                    flags.LINKFLAGS_LINK += GET_FLAG_EVALUATE(
                        AND(AddressSanitizer::ON, RuntimeLink::SHARED),
                        R"(/wholearchive:"clang_rt.asan_dynamic-x86_64.lib
           /wholearchive:"clang_rt.asan_dynamic_runtime_thunk-x86_64.lib )"); flags.FINDLIBS_SA_LINK +=
                        GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC, TargetType::EXECUTABLE),
                                          "clang_rt.asan-x86_64 clang_rt.asan_cxx-x86_64 ");
                    flags.LINKFLAGS_LINK += GET_FLAG_EVALUATE(
                        AND(AddressSanitizer::ON, RuntimeLink::STATIC, TargetType::EXECUTABLE),
                        R"(/wholearchive:"clang_rt.asan-x86_64.lib /wholearchive:"clang_rt.asan_cxx-x86_64.lib ")");
                    string FINDLIBS_SA_LINK_DLL =
                        GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC),
           "clang_rt.asan_dll_thunk-x86_64 "); string LINKFLAGS_LINK_DLL = GET_FLAG_EVALUATE(AND(AddressSanitizer::ON,
           RuntimeLink::STATIC), R"(/wholearchive:"clang_rt.asan_dll_thunk-x86_64.lib ")");
                }
                else if (EVALUATE(AddressModel::A_32))
                {
                    // The various 32 bit runtime asan support libraries and related flags.

                    flags.FINDLIBS_SA_LINK =
                        GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::SHARED),
                                          "clang_rt.asan_dynamic-i386 clang_rt.asan_dynamic_runtime_thunk-i386 ");
                    flags.LINKFLAGS_LINK += GET_FLAG_EVALUATE(
                        AND(AddressSanitizer::ON, RuntimeLink::SHARED),
                        R"(/wholearchive:"clang_rt.asan_dynamic-i386.lib
           /wholearchive:"clang_rt.asan_dynamic_runtime_thunk-i386.lib )"); flags.FINDLIBS_SA_LINK +=
                        GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC, TargetType::EXECUTABLE),
                                          "clang_rt.asan-i386 clang_rt.asan_cxx-i386 ");
                    flags.LINKFLAGS_LINK += GET_FLAG_EVALUATE(
                        AND(AddressSanitizer::ON, RuntimeLink::STATIC, TargetType::EXECUTABLE),
                        R"(/wholearchive:"clang_rt.asan-i386.lib /wholearchive:"clang_rt.asan_cxx-i386.lib ")");
                    string FINDLIBS_SA_LINK_DLL =
                        GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC), "clang_rt.asan_dll_thunk-i386
           "); string LINKFLAGS_LINK_DLL = GET_FLAG_EVALUATE(AND(AddressSanitizer::ON, RuntimeLink::STATIC),
                                                                  R"(/wholearchive:"clang_rt.asan_dll_thunk-i386.lib
           ")");
                }*/

        flags.LINKFLAGS_LINK += GET_FLAG_EVALUATE(Arch::IA64, "/MACHINE:IA64 ");
        if (EVALUATE(Arch::X86))
        {
            flags.LINKFLAGS_LINK +=
                GET_FLAG_EVALUATE(AddressModel::A_64, "/MACHINE:X64 ", AddressModel::A_32, "/MACHINE:X86 ");
        }
        else if (EVALUATE(Arch::ARM))
        {
            flags.LINKFLAGS_LINK +=
                GET_FLAG_EVALUATE(AddressModel::A_64, "/MACHINE:ARM64 ", AddressModel::A_32, "/MACHINE:ARM ");
        }

        // Rule register-toolset-really on Line 1852
        SINGLE(RuntimeLink::SHARED, Threading::MULTI);
        // TODO
        // debug-store and pch-source features are being added. don't know where it will be used so holding back

        // TODO Line 1916 PCH Related Variables are not being set

        // TODO:
        // Line 1927 - 1930 skipped because of cpu-type

        flags.PDB_CFLAG += GET_FLAG_EVALUATE(AND(DebugSymbols::ON, DebugStore::DATABASE), "/Fd ");

        // TODO// Line 1971
        //  There are variables UNDEFS and FORCE_INCLUDES

        if (EVALUATE(DebugSymbols::ON))
        {
            flags.PDB_LINKFLAG += GET_FLAG_EVALUATE(DebugStore::DATABASE, "/PDB: ");
            flags.LINKFLAGS_LINK += "/DEBUG ";
            flags.LINKFLAGS_MSVC += GET_FLAG_EVALUATE(RuntimeDebugging::OFF, "/OPT:REF,ICF  ");
        }
        flags.LINKFLAGS_MSVC +=
            GET_FLAG_EVALUATE(UserInterface::CONSOLE, "/subsystem:console ", UserInterface::GUI, "/subsystem:windows ",
                              UserInterface::WINCE, "/subsystem:windowsce ", UserInterface::NATIVE,
                              "/subsystem:native ", UserInterface::AUTO, "/subsystem:posix ");
    }
    else if (linker.bTFamily == BTFamily::GCC)
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
        //

        auto addToBothCOMPILE_FLAGS_and_LINK_FLAGS = [&flags](const string &str) { flags.OPTIONS_LINK += str; };
        auto addToBothOPTIONS_COMPILE_CPP_and_OPTIONS_LINK = [&flags](const string &str) { flags.OPTIONS_LINK += str; };

        // FINDLIBS-SA variable is being updated gcc.link rule.
        string findLibsSA;

        auto isTargetBSD = [&]() { return OR(TargetOS::BSD, TargetOS::FREEBSD, TargetOS::NETBSD, TargetOS::NETBSD); };
        {
            // -fPIC
            if (linkTargetType == TargetType::LIBRARY_STATIC || linkTargetType == TargetType::LIBRARY_SHARED)
            {
                if (!OR(TargetOS::WINDOWS, TargetOS::CYGWIN))
                {
                    addToBothCOMPILE_FLAGS_and_LINK_FLAGS("-fPIC ");
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
                if (linker.bTVersion >= Version{10})
                {
                    setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_20);
                }
                else if (linker.bTVersion >= Version{8})
                {
                    setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_2a);
                }
                else if (linker.bTVersion >= Version{6})
                {
                    setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_17);
                }
                else if (linker.bTVersion >= Version{5})
                {
                    setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_1z);
                }
                else if (linker.bTVersion >= Version{4, 9})
                {
                    setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_14);
                }
                else if (linker.bTVersion >= Version{4, 8})
                {
                    setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_1y);
                }
                else if (linker.bTVersion >= Version{4, 7})
                {
                    setCppStdAndDialectCompilerAndLinkerFlags(CxxSTD::V_11);
                }
                else if (linker.bTVersion >= Version{3, 3})
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

        // LTO
        if (EVALUATE(LTO::ON))
        {
            flags.OPTIONS_LINK += GET_FLAG_EVALUATE(LTOMode::FULL, "-flto ");
        }

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

        // Linker Flags

        flags.OPTIONS_LINK += GET_FLAG_EVALUATE(DebugSymbols::ON, "-g ");
        flags.OPTIONS_LINK += GET_FLAG_EVALUATE(Profiling::ON, "-pg ");

        flags.OPTIONS_LINK +=
            GET_FLAG_EVALUATE(LocalVisibility::HIDDEN, "-fvisibility=hidden -fvisibility-inlines-hidden ",
                              LocalVisibility::GLOBAL, "-fvisibility=default ");
        if (!EVALUATE(TargetOS::DARWIN))
        {
            flags.OPTIONS_LINK += GET_FLAG_EVALUATE(LocalVisibility::PROTECTED, "-fvisibility=protected ");
        }

        // Sanitizers
        // Though sanitizer flags for compiler are assigned at different location, and are for c++, but are exact same
        flags.OPTIONS_LINK += sanitizerFlags;
        flags.OPTIONS_LINK += GET_FLAG_EVALUATE(Coverage::ON, "--coverage ");

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
        flags.OPTIONS_LINK += GET_FLAG_EVALUATE(TargetOS::AIX, "-Wl -bnoipath -Wl -bbigtoc ");
        flags.OPTIONS_LINK += GET_FLAG_EVALUATE(AND(TargetOS::AIX, RuntimeLink::STATIC), "-static ");
        //  On Darwin, the -s option to ld does not work unless we pass -static,
        // and passing -static unconditionally is a bad idea. So, do not pass -s
        // at all and darwin.jam will use a separate 'strip' invocation.

        flags.RPATH_OPTION_LINK += GET_FLAG_EVALUATE(AND(TargetOS::DARWIN, RuntimeLink::STATIC), "-static ");

        // For TargetOS::VSWORKS, Environment Variables are also considered, but here they aren't
        flags.OPTIONS_LINK += GET_FLAG_EVALUATE(AND(TargetOS::VXWORKS, Strip::ON), "--strip-all ");

        auto isGenericOS = [&]() {
            return !OR(TargetOS::AIX, TargetOS::DARWIN, TargetOS::VXWORKS, TargetOS::SOLARIS, TargetOS::OSF,
                       TargetOS::HPUX, TargetOS::IPHONE, TargetOS::APPLETV);
        };

        flags.OPTIONS_LINK += GET_FLAG_EVALUATE(AND(isGenericOS(), Strip::ON), "--strip-all ");

        auto isRpathOs = [&]() {
            return !OR(TargetOS::AIX, TargetOS::DARWIN, TargetOS::VXWORKS, TargetOS::SOLARIS, TargetOS::OSF,
                       TargetOS::HPUX, TargetOS::WINDOWS);
        };

        flags.RPATH_OPTION_LINK += GET_FLAG_EVALUATE(isRpathOs(), "-rpath ");

        string bStatic = "-Bstatic ";
        string bDynamic = "-Bdynamic ";

        flags.FINDLIBS_ST_PFX_LINK += GET_FLAG_EVALUATE(AND(isGenericOS(), RuntimeLink::SHARED), bStatic);
        flags.FINDLIBS_SA_PFX_LINK += GET_FLAG_EVALUATE(AND(isGenericOS(), RuntimeLink::SHARED), bDynamic);

        if (AND(TargetOS::WINDOWS, RuntimeLink::STATIC))
        {
            flags.FINDLIBS_ST_PFX_LINK += bStatic;
            flags.FINDLIBS_SA_PFX_LINK += bDynamic;
            flags.OPTIONS_LINK += bStatic;
        }
        flags.SONAME_OPTION_LINK += GET_FLAG_EVALUATE(isGenericOS(), "-h ");

        flags.OPTIONS_LINK += GET_FLAG_EVALUATE(AND(isGenericOS(), RuntimeLink::STATIC), "-static ");
        flags.OPTIONS_LINK += GET_FLAG_EVALUATE(AND(TargetOS::HPUX, Strip::ON), "-s ");

        flags.SONAME_OPTION_LINK += GET_FLAG_EVALUATE(TargetOS::HPUX, "+h ");

        flags.OPTIONS_LINK += GET_FLAG_EVALUATE(AND(TargetOS::OSF, Strip::ON), "-s ");

        flags.RPATH_OPTION_LINK += GET_FLAG_EVALUATE(TargetOS::OSF, "-rpath ");

        flags.OPTIONS_LINK += GET_FLAG_EVALUATE(AND(TargetOS::OSF, RuntimeLink::STATIC), "-static ");

        flags.OPTIONS_LINK += GET_FLAG_EVALUATE(AND(TargetOS::SOLARIS, Strip::ON), "-s ");

        if (EVALUATE(TargetOS::SOLARIS))
        {
            flags.OPTIONS_LINK += "-mimpure-text ";
            flags.OPTIONS_LINK += GET_FLAG_EVALUATE(RuntimeLink::STATIC, "-static ");
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

string LinkOrArchiveTarget::getLinkOrArchiveCommand(bool ignoreTargets)
{
    if (linkOrArchiveCommand.empty() || ignoreTargets)
    {
        string localLinkOrArchiveCommand;
        if (linkTargetType == TargetType::LIBRARY_STATIC)
        {
            localLinkOrArchiveCommand = archiver.bTFamily == BTFamily::MSVC ? "/nologo " : "";
            auto getArchiveOutputFlag = [&]() -> string {
                if (archiver.bTFamily == BTFamily::MSVC)
                {
                    return "/OUT:";
                }
                else if (archiver.bTFamily == BTFamily::GCC)
                {
                    return " rcs ";
                }
                return "";
            };
            localLinkOrArchiveCommand += getArchiveOutputFlag();
            localLinkOrArchiveCommand += addQuotes(getActualOutputPath()) + " ";
        }
        else
        {
            localLinkOrArchiveCommand = linker.bTFamily == BTFamily::MSVC ? " /NOLOGO " : "";
            LinkerFlags flags = getLinkerFlags();

            // TODO Not catering for MSVC
            // Temporary Just for ensuring link success with clang Address-Sanitizer
            // There should be no spaces after user-provided-flags.
            // TODO shared libraries not supported.
            if (linker.bTFamily == BTFamily::GCC)
            {
                localLinkOrArchiveCommand += flags.OPTIONS + " " + flags.OPTIONS_LINK + " ";
            }
            else if (linker.bTFamily == BTFamily::MSVC)
            {
                for (const string &str : split(flags.FINDLIBS_SA_LINK, " "))
                {
                    if (str.empty())
                    {
                        continue;
                    }
                    localLinkOrArchiveCommand += str + ".lib ";
                }
                localLinkOrArchiveCommand += flags.LINKFLAGS_LINK + flags.LINKFLAGS_MSVC;
            }
            localLinkOrArchiveCommand += requirementLinkerFlags + " ";
        }

        auto getLinkFlag = [this](const string &libraryPath, const string &libraryName) {
            if (linker.bTFamily == BTFamily::MSVC)
            {
                return addQuotes(libraryPath + libraryName + ".lib") + " ";
            }
            else
            {
                return "-L" + addQuotes(libraryPath) + " -l" + addQuotes(libraryName) + " ";
            }
        };

        if (!ignoreTargets)
        {

            for (ObjectFile *objectFile : objectFiles)
            {
                localLinkOrArchiveCommand += addQuotes(objectFile->getObjectFileOutputFilePath()) + " ";
            }

            for (PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget : requirementDeps)
            {
                localLinkOrArchiveCommand +=
                    getLinkFlag(prebuiltLinkOrArchiveTarget->outputDirectory, prebuiltLinkOrArchiveTarget->outputName);
            }
        }

        auto getLibraryDirectoryFlag = [this]() {
            if (linker.bTFamily == BTFamily::MSVC)
            {
                return "/LIBPATH:";
            }
            else
            {
                return "-L";
            }
        };

        // Following set is needed because otherwise pointers don't have string-like ordering, so two runs may
        // generate different link-commands, thus hurting the caching.
        set<string> libraryIncludes;

        for (const Node *includeDir : libraryDirectories)
        {
            libraryIncludes.emplace(includeDir->filePath);
        }
        for (const Node *stdLibDir : standardLibraryDirectories)
        {
            libraryIncludes.emplace(stdLibDir->filePath);
        }
        for (const string &libDir : libraryIncludes)
        {
            localLinkOrArchiveCommand += getLibraryDirectoryFlag() + addQuotes(libDir) + " ";
        }
        localLinkOrArchiveCommand += linker.bTFamily == BTFamily::MSVC ? " /OUT:" : " -o ";
        localLinkOrArchiveCommand += addQuotes(getActualOutputPath()) + " ";

        if (linkTargetType == TargetType::LIBRARY_SHARED)
        {
            localLinkOrArchiveCommand += linker.bTFamily == BTFamily::MSVC ? "/DLL  " : " -shared ";
        }

        if (ignoreTargets)
        {
            return localLinkOrArchiveCommand;
        }
        else
        {
            linkOrArchiveCommand = localLinkOrArchiveCommand;
        }
    }
    return linkOrArchiveCommand;
}

string &LinkOrArchiveTarget::getLinkOrArchiveCommandPrint()
{
    if (linkOrArchiveCommandPrint.empty())
    {
        setLinkOrArchiveCommandPrint();
    }

    return linkOrArchiveCommandPrint;
}

void LinkOrArchiveTarget::setJson()
{
    Json targetJson;
    targetJson[JConsts::targetType] = linkTargetType;
    targetJson[JConsts::outputName] = outputName;
    targetJson[JConsts::outputDirectory] = outputDirectory;
    targetJson[JConsts::linker] = linker;
    targetJson[JConsts::archiver] = archiver;
    targetJson[JConsts::linkerFlags] = requirementLinkerFlags;
    vector<Json> requirementDepsJson;
    requirementDepsJson.reserve(requirementDeps.size());
    for (DS<PrebuiltLinkOrArchiveTarget> *prebuilt : requirementDeps)
    {
        requirementDepsJson.emplace_back(static_cast<PrebuiltLinkOrArchiveTarget *>(prebuilt)->getTarjanNodeName());
    }
    targetJson[JConsts::libraryDependencies] = requirementDepsJson;
    json[0] = std::move(targetJson);
}

BTarget *LinkOrArchiveTarget::getBTarget()
{
    return this;
}

string LinkOrArchiveTarget::getTarjanNodeName()
{
    string str;
    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        str = "Static Library";
    }
    else if (linkTargetType == TargetType::LIBRARY_SHARED)
    {
        str = "Shared Library";
    }
    else
    {
        str = "Executable";
    }
    return str + " " + getSubDirForTarget();
}

PostBasic LinkOrArchiveTarget::Archive()
{
    return PostBasic(archiver, getLinkOrArchiveCommand(false), getLinkOrArchiveCommandPrint(), buildCacheFilesDirPath,
                     name, settings.acpSettings.outputAndErrorFiles, true);
}

PostBasic LinkOrArchiveTarget::Link()
{
    return PostBasic(linker, getLinkOrArchiveCommand(false), getLinkOrArchiveCommandPrint(), buildCacheFilesDirPath,
                     name, settings.lcpSettings.outputAndErrorFiles, true);
}

void LinkOrArchiveTarget::setLinkOrArchiveCommandPrint()
{
    const ArchiveCommandPrintSettings &acpSettings = settings.acpSettings;
    const LinkCommandPrintSettings &lcpSettings = settings.lcpSettings;

    if (linkTargetType == TargetType::LIBRARY_STATIC)
    {
        if (acpSettings.tool.printLevel != PathPrintLevel::NO)
        {
            linkOrArchiveCommandPrint +=
                getReducedPath(archiver.bTPath.make_preferred().string(), acpSettings.tool) + " ";
        }

        if (acpSettings.infrastructureFlags)
        {
            linkOrArchiveCommandPrint += archiver.bTFamily == BTFamily::MSVC ? "/nologo " : "";
        }
        auto getArchiveOutputFlag = [&]() -> string {
            if (archiver.bTFamily == BTFamily::MSVC)
            {
                return "/OUT:";
            }
            else if (archiver.bTFamily == BTFamily::GCC)
            {
                return " rcs ";
            }
            return "";
        };
        if (acpSettings.infrastructureFlags)
        {
            linkOrArchiveCommandPrint += getArchiveOutputFlag();
        }

        if (acpSettings.archive.printLevel != PathPrintLevel::NO)
        {
            linkOrArchiveCommandPrint += getReducedPath(getActualOutputPath(), acpSettings.archive) + " ";
        }
    }
    else
    {
        if (lcpSettings.tool.printLevel != PathPrintLevel::NO)
        {
            linkOrArchiveCommandPrint +=
                getReducedPath(linker.bTPath.make_preferred().string(), lcpSettings.tool) + " ";
        }

        if (lcpSettings.infrastructureFlags)
        {
            linkOrArchiveCommandPrint += linker.bTFamily == BTFamily::MSVC ? " /NOLOGO " : "";
        }

        LinkerFlags flags = getLinkerFlags();

        if (lcpSettings.infrastructureFlags)
        {
            // TODO Not catering for MSVC
            // Temporary Just for ensuring link success with clang Address-Sanitizer
            // There should be no spaces after user-provided-flags.
            // TODO shared libraries not supported.
            if (linker.bTFamily == BTFamily::GCC)
            {
                linkOrArchiveCommandPrint += flags.OPTIONS + " " + flags.OPTIONS_LINK + " ";
            }
            else if (linker.bTFamily == BTFamily::MSVC)
            {

                for (const string &str : split(flags.FINDLIBS_SA_LINK, " "))
                {
                    if (str.empty())
                    {
                        continue;
                    }
                    linkOrArchiveCommandPrint += str + ".lib ";
                }
                linkOrArchiveCommandPrint += flags.LINKFLAGS_LINK + flags.LINKFLAGS_MSVC;
            }
        }

        if (lcpSettings.linkerFlags)
        {
            linkOrArchiveCommandPrint += requirementLinkerFlags + " ";
        }
    }

    auto getLinkFlag = [this](const string &libraryPath, const string &libraryName) {
        if (linker.bTFamily == BTFamily::MSVC)
        {
            return addQuotes(libraryPath + libraryName + ".lib") + " ";
        }
        else
        {
            return "-L" + addQuotes(libraryPath) + " -l" + addQuotes(libraryName) + " ";
        }
    };

    auto getLinkFlagPrint = [this](const string &libraryPath, const string &libraryName, const PathPrint &pathPrint) {
        if (linker.bTFamily == BTFamily::MSVC)
        {
            return getReducedPath(libraryPath + libraryName + ".lib", pathPrint) + " ";
        }
        else
        {
            return "-L" + getReducedPath(libraryPath, pathPrint) + " -l" + getReducedPath(libraryName, pathPrint) + " ";
        }
    };

    {
        const PathPrint *pathPrint;
        if (linkTargetType == TargetType::LIBRARY_STATIC)
        {
            pathPrint = &(acpSettings.objectFiles);
        }
        else
        {
            pathPrint = &(lcpSettings.objectFiles);
        }
        if (pathPrint->printLevel != PathPrintLevel::NO)
        {
            for (ObjectFile *objectFile : objectFiles)
            {
                linkOrArchiveCommandPrint += objectFile->getObjectFileOutputFilePathPrint(*pathPrint) + " ";
            }
        }
    }

    for (PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget : requirementDeps)
    {
        linkOrArchiveCommandPrint +=
            getLinkFlagPrint(prebuiltLinkOrArchiveTarget->outputDirectory, prebuiltLinkOrArchiveTarget->outputName,
                             lcpSettings.libraryDependencies);
    }

    // HMake does not link any dependency to static library

    auto getLibraryDirectoryFlag = [this]() {
        if (linker.bTFamily == BTFamily::MSVC)
        {
            return "/LIBPATH:";
        }
        else
        {
            return "-L";
        }
    };

    if (lcpSettings.libraryDirectories.printLevel != PathPrintLevel::NO)
    {
        for (const Node *includeDir : libraryDirectories)
        {
            linkOrArchiveCommandPrint +=
                getLibraryDirectoryFlag() + getReducedPath(includeDir->filePath, lcpSettings.libraryDirectories) + " ";
        }
    }

    if (lcpSettings.standardLibraryDirectories.printLevel != PathPrintLevel::NO)
    {
        for (const Node *stdLibDir : standardLibraryDirectories)
        {

            linkOrArchiveCommandPrint += getLibraryDirectoryFlag() +
                                         getReducedPath(stdLibDir->filePath, lcpSettings.standardLibraryDirectories) +
                                         " ";
        }
    }

    if (lcpSettings.infrastructureFlags)
    {
        linkOrArchiveCommandPrint += linker.bTFamily == BTFamily::MSVC ? " /OUT:" : " -o ";
    }

    if (lcpSettings.binary.printLevel != PathPrintLevel::NO)
    {
        linkOrArchiveCommandPrint += getReducedPath(getActualOutputPath(), lcpSettings.binary) + " ";
    }

    if (lcpSettings.infrastructureFlags)
    {
        linkOrArchiveCommandPrint += linker.bTFamily == BTFamily::MSVC ? "/DLL" : " -shared ";
    }
}

void LinkOrArchiveTarget::checkForPreBuiltAndCacheDir(Builder &)
{
    if (!exists(path(buildCacheFilesDirPath)))
    {
        create_directories(buildCacheFilesDirPath);
    }

    RealBTarget &realBTarget = getRealBTarget(0);

    realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
    if (exists(path(buildCacheFilesDirPath) / (name + ".cache")))
    {
        Json linkTargetCacheJson;
        ifstream(path(buildCacheFilesDirPath) / (name + ".cache")) >> linkTargetCacheJson;
        string command = std::move(linkTargetCacheJson.at(JConsts::linkCommand));
        cachedObjectFiles = std::move(linkTargetCacheJson.at(JConsts::objectFiles).get<set<string>>());
        path targetOutputPath = getActualOutputPath();

        if (exists(targetOutputPath) && command == linker.bTPath.generic_string() + " " + getLinkOrArchiveCommand(true))
        {
            realBTarget.fileStatus = FileStatus::UPDATED;
        }
    }
}