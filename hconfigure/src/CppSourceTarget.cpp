
#ifdef USE_HEADER_UNITS
import "CppSourceTarget.hpp";
import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "ConfigHelpers.hpp";
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
#include "ConfigHelpers.hpp"
#include "LinkOrArchiveTarget.hpp"
#include "Utilities.hpp"
#include <filesystem>
#include <fstream>
#include <regex>
#include <utility>
#endif

using std::filesystem::create_directories, std::filesystem::directory_iterator,
    std::filesystem::recursive_directory_iterator, std::ifstream, std::ofstream, std::regex, std::regex_error;

SourceDirectory::SourceDirectory(const pstring &sourceDirectory_, pstring regex_, const bool recursive_)
    : sourceDirectory{Node::getNodeFromNonNormalizedPath(sourceDirectory_, false)}, regex{std::move(regex_)},
      recursive(recursive_)
{
}

bool InclNodePointerComparator::operator()(const InclNode &lhs, const InclNode &rhs) const
{
    return lhs.node < rhs.node;
}

InclNodeRecord::InclNodeRecord(Node *node_)
{
}

ResolveRequirePathBTarget::ResolveRequirePathBTarget(CppSourceTarget *target_) : target(target_)
{
}

void ResolveRequirePathBTarget::updateBTarget(Builder &builder, const unsigned short round)
{
    if (round == 1 && realBTargets[1].exitStatus == EXIT_SUCCESS)
    {
        target->resolveRequirePaths();
    }
}

pstring ResolveRequirePathBTarget::getTarjanNodeName() const
{
    return "ResolveRequirePath " + target->name;
}

AdjustHeaderUnitsBTarget::AdjustHeaderUnitsBTarget(CppSourceTarget *target_) : target(target_)
{
}

void AdjustHeaderUnitsBTarget::updateBTarget(Builder &builder, const unsigned short round)
{
    if (round == 1)
    {
        target->adjustHeaderUnitsPValueArrayPointers();

        if (target->targetCacheChanged.load())
        {
            target->targetCacheChanged.store(false);
            target->saveBuildCache(true);
        }
    }
}

pstring AdjustHeaderUnitsBTarget::getTarjanNodeName() const
{
    return "AdjustHeaderUnitsBTarget " + target->name;
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

CppSourceTarget::CppSourceTarget(pstring name_, const TargetType targetType) : CSourceTarget{false, std::move(name_)}
{
    initializeCppSourceTarget(targetType);
}

CppSourceTarget::CppSourceTarget(bool buildExplicit, pstring name_, const TargetType targetType)
    : CSourceTarget{buildExplicit, std::move(name_)}
{
    initializeCppSourceTarget(targetType);
}

void CppSourceTarget::initializeCppSourceTarget(TargetType targetType)
{
    compileTargetType = targetType;
    targetConfigCache->PushBack(kArrayType, ralloc)
        .PushBack(kArrayType, ralloc)
        .PushBack(kArrayType, ralloc)
        .PushBack(kArrayType, ralloc)
        .PushBack(kArrayType, ralloc)
        .PushBack(kArrayType, ralloc);
}

void CppSourceTarget::getObjectFiles(vector<const ObjectFile *> *objectFiles,
                                     LinkOrArchiveTarget *linkOrArchiveTarget) const
{
    set<const SMFile *, IndexInTopologicalSortComparatorRoundZero> sortedSMFileDependencies;
    for (const SMFile &objectFile : modFileDeps)
    {
        sortedSMFileDependencies.emplace(&objectFile);
    }
    for (const SMFile &headerUnit : headerUnits)
    {
        sortedSMFileDependencies.emplace(&headerUnit);
    }

    for (const SMFile *objectFile : sortedSMFileDependencies)
    {
        objectFiles->emplace_back(objectFile);
    }

    for (const SourceNode &objectFile : srcFileDeps)
    {
        objectFiles->emplace_back(&objectFile);
    }
}

void CppSourceTarget::populateTransitiveProperties()
{
    for (CSourceTarget *cSourceTarget : requirementDeps)
    {
        for (const InclNode &inclNode : cSourceTarget->useReqIncls)
        {
            actuallyAddInclude(reqIncls, inclNode.node->filePath, inclNode.isStandard, inclNode.ignoreHeaderDeps);
        }
        requirementCompilerFlags += cSourceTarget->usageRequirementCompilerFlags;
        for (const Define &define : cSourceTarget->usageRequirementCompileDefinitions)
        {
            requirementCompileDefinitions.emplace(define);
        }
        requirementCompilerFlags += cSourceTarget->usageRequirementCompilerFlags;
        if (cSourceTarget->getCSourceTargetType() == CSourceTargetType::CppSourceTarget)
        {
            CppSourceTarget *cppSourceTarget = static_cast<CppSourceTarget *>(cSourceTarget);
            for (InclNodeTargetMap &inclNodeTargetMap : cppSourceTarget->useReqHuDirs)
            {
                bool found = false;
                for (const InclNodeTargetMap &inclNodeTargetMap_ : reqHuDirs)
                {
                    if (inclNodeTargetMap_.inclNode.node->myId == inclNodeTargetMap.inclNode.node->myId)
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                {
                    printErrorMessageColor(
                        fmt::format("Include Directory\n{}\nbelongs to two different target\n{}\nand\n{}\n",
                                    inclNodeTargetMap.inclNode.node->filePath, getTarjanNodeName(),
                                    inclNodeTargetMap.cppSourceTarget->getTarjanNodeName()),
                        settings.pcSettings.toolErrorOutput);
                    throw std::exception();
                    // Print Error Message that same include-directory belongs to two targets.
                }
                reqHuDirs.emplace_back(inclNodeTargetMap);
            }
            if (!cppSourceTarget->useReqHuDirs.empty())
            {
                cppSourceTarget->adjustHeaderUnitsBTarget.realBTargets[1].addDependency(adjustHeaderUnitsBTarget);
            }
        }
    }
}

void CppSourceTarget::adjustHeaderUnitsPValueArrayPointers()
{
    // All header-units are found, so header-units pvalue array size could be reserved
    // If a new header-unit was added in this run, sourceJson pointers are adjusted
    if (newHeaderUnitsSize)
    {
        PValue &headerUnitsPValueArray = (*targetBuildCache)[2];
        headerUnitsPValueArray.Reserve(headerUnitsPValueArray.Size() + newHeaderUnitsSize, cppAllocator);

        for (const SMFile &headerUnit : headerUnits)
        {
            if (headerUnit.headerUnitsIndex == UINT64_MAX)
            {
                // headerUnit did not exist before in the cache
                const PValue *oldPtr = headerUnit.sourceJson;

                // old value is moved
                headerUnitsPValueArray.PushBack(*headerUnit.sourceJson, cppAllocator);

                // old pointer cleared
                delete oldPtr;

                // Reassigning the old value moved in the array
                const_cast<SMFile &>(headerUnit).sourceJson = headerUnitsPValueArray.End() - 1;
            }
            else
            {
                // headerUnit existed before in the cache but because array size is increased the sourceJson
                // pointer is invalidated now. Reassigning the pointer based on previous index.
                const_cast<SMFile &>(headerUnit).sourceJson = &headerUnitsPValueArray[headerUnit.headerUnitsIndex];
            }
        }
    }
}

CSourceTargetType CppSourceTarget::getCSourceTargetType() const
{
    return CSourceTargetType::CppSourceTarget;
}

CppSourceTarget &CppSourceTarget::makeReqInclsUseable()
{
    if (bsMode == BSMode::BUILD && useMiniTarget == UseMiniTarget::YES)
    {
        // Initialized in CppSourceTarget round 2
    }
    else
    {
        for (const InclNode &include : reqIncls)
        {
            if (evaluate(TreatModuleAsSource::NO))
            {
                actuallyAddInclude(this, reqHuDirs, include.node->filePath, include.isStandard,
                                   include.ignoreHeaderDeps);
                actuallyAddInclude(this, useReqHuDirs, include.node->filePath, include.isStandard,
                                   include.ignoreHeaderDeps);
                actuallyAddInclude(useReqIncls, include.node->filePath, include.isStandard, include.ignoreHeaderDeps);
            }
            else
            {
                actuallyAddInclude(useReqIncls, include.node->filePath, include.isStandard, include.ignoreHeaderDeps);
            }
        }
    }

    return *this;
}

bool CppSourceTarget::actuallyAddSourceFile(vector<SourceNode> &sourceFiles, const pstring &sourceFile,
                                            CppSourceTarget *target)
{
    Node *node = Node::getNodeFromNonNormalizedPath(sourceFile, true);
    return actuallyAddSourceFile(sourceFiles, node, target);
}

bool CppSourceTarget::actuallyAddSourceFile(vector<SourceNode> &sourceFiles, Node *sourceFileNode,
                                            CppSourceTarget *target)
{
    bool found = false;
    for (const SourceNode &source : sourceFiles)
    {
        if (source.node == sourceFileNode)
        {
            found = true;
            printErrorMessage(fmt::format("Source File {} already added in target {}.\n", sourceFileNode->filePath,
                                          source.target->name));
            break;
        }
    }
    if (!found)
    {
        sourceFiles.emplace_back(target, sourceFileNode);
        return true;
    }
    return false;
}

bool CppSourceTarget::actuallyAddModuleFile(vector<SMFile> &smFiles, const pstring &moduleFile, CppSourceTarget *target)
{
    Node *node = Node::getNodeFromNonNormalizedPath(moduleFile, true);
    return actuallyAddModuleFile(smFiles, node, target);
}

bool CppSourceTarget::actuallyAddModuleFile(vector<SMFile> &smFiles, Node *moduleFileNode, CppSourceTarget *target)
{
    bool found = false;
    for (const SMFile &smFile : smFiles)
    {
        if (smFile.node == moduleFileNode)
        {
            found = true;
            printErrorMessage(
                fmt::format("Module File {} already added in target {}.\n", moduleFileNode->filePath, target->name));
            break;
        }
    }
    if (!found)
    {
        smFiles.emplace_back(target, moduleFileNode);
        return true;
    }
    return false;
}

CppSourceTarget &CppSourceTarget::removeSourceFile(const pstring &sourceFile)
{
    if (bsMode == BSMode::CONFIGURE)
    {
        const Node *node = Node::getNodeFromNonNormalizedPath(sourceFile, true);
        PValue &value = targetConfigCache[0][Indices::CppTargetConfigCache::sourceFiles].GetArray();
        bool removed = false;
        for (auto it = value.Begin(); it != value.End(); ++it)
        {
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
            if (it->GetUint64() == node->myId)
#else
            if (compareStringsFromEnd(pstring_view(it->GetString(), it->GetStringLength()), node->filePath))
#endif
            {
                value.Erase(it, it + 1);
                removed = true;
                break;
            }
        }
        if (!removed)
        {
            printErrorMessage(
                fmt::format("Could Not remove {} from source-files in target {}.\n", node->filePath, name));
        }
    }
    return *this;
}
CppSourceTarget &CppSourceTarget::removeModuleFile(const pstring &moduleFile)
{
    if (evaluate(TreatModuleAsSource::YES))
    {
        return removeSourceFile(moduleFile);
    }
    if (bsMode == BSMode::CONFIGURE)
    {
        const Node *node = Node::getNodeFromNonNormalizedPath(moduleFile, true);
        PValue &value = targetConfigCache[0][Indices::CppTargetConfigCache::moduleFiles].GetArray();
        bool removed = false;
        for (auto it = value.Begin(); it != value.End(); it = it + 2)
        {
#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
            if (it->GetUint64() == node->myId)
#else
            if (compareStringsFromEnd(pstring_view(it->GetString(), it->GetStringLength()), node->filePath))
#endif
            {
                value.Erase(it, it + 2);
                removed = true;
                break;
            }
        }
        if (!removed)
        {
            printErrorMessage(fmt::format("Could Not remove {} from module-files in target {}.\n", moduleFile, name));
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
        pstring defaultAssembler = evaluate(Arch::IA64) ? "ias " : "";
        if (evaluate(Arch::X86))
        {
            defaultAssembler += GET_FLAG_evaluate(AddressModel::A_64, "ml64 ", AddressModel::A_32, "ml -coff ");
        }
        else if (evaluate(Arch::ARM))
        {
            defaultAssembler += GET_FLAG_evaluate(AddressModel::A_64, "armasm64 ", AddressModel::A_32, "armasm ");
        }
        pstring assemblerFlags = GET_FLAG_evaluate(OR(Arch::X86, Arch::IA64), "-c -Zp4 -Cp -Cx ");
        pstring assemblerOutputFlag = GET_FLAG_evaluate(OR(Arch::X86, Arch::IA64), "-Fo ", Arch::ARM, "-o ");
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
            pstring FINDLIBS_SA_LINK =
                GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::SHARED),
                                  "clang_rt.asan_dynamic-x86_64 clang_rt.asan_dynamic_runtime_thunk-x86_64 ");
            FINDLIBS_SA_LINK +=
                GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::STATIC, TargetType::EXECUTABLE),
                                  "clang_rt.asan-x86_64 clang_rt.asan_cxx-x86_64 ");
            pstring FINDLIBS_SA_LINK_DLL =
                GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::STATIC), "clang_rt.asan_dll_thunk-x86_64 ");
            pstring LINKFLAGS_LINK_DLL = GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::STATIC),
                                                           R"(/wholearchive\:"clang_rt.asan_dll_thunk-x86_64.lib ")");
        }
        else if (evaluate(AddressModel::A_32))
        {
            // The various 32 bit runtime asan support libraries and related flags.

            pstring FINDLIBS_SA_LINK =
                GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::SHARED),
                                  "clang_rt.asan_dynamic-i386 clang_rt.asan_dynamic_runtime_thunk-i386 ");
            FINDLIBS_SA_LINK +=
                GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::STATIC, TargetType::EXECUTABLE),
                                  "clang_rt.asan-i386 clang_rt.asan_cxx-i386 ");
            pstring FINDLIBS_SA_LINK_DLL =
                GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::STATIC), "clang_rt.asan_dll_thunk-i386 ");
            pstring LINKFLAGS_LINK_DLL = GET_FLAG_evaluate(AND(AddressSanitizer::ON, RuntimeLink::STATIC),
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
        SINGLE(RuntimeLink::SHARED, Threading::MULTI);
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

        auto addToBothCOMPILE_FLAGS_and_LINK_FLAGS = [&flags](const pstring &str) { flags.OPTIONS_COMPILE += str; };
        auto addToBothOPTIONS_COMPILE_CPP_and_OPTIONS_LINK = [&flags](const pstring &str) {
            flags.OPTIONS_COMPILE_CPP += str;
        };

        // FINDLIBS-SA variable is being updated gcc.link rule.
        pstring findLibsSA;

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
        pstring sanitizerFlags;
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
            if (noStaticLink && evaluate(RuntimeLink::STATIC))
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
            }
        }

        pstring str = GET_FLAG_evaluate(
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

void CppSourceTarget::updateRound1()
{
}

void CppSourceTarget::updateBTarget(Builder &builder, const unsigned short round)
{
    if (!round)
    {
        if (targetCacheChanged.load())
        {
            targetCacheChanged.store(false);
            saveBuildCache(round);
            assignFileStatusToDependents(realBTargets[0]);
        }
    }
    else if (round == 1)
    {
        populateSourceNodes();
    }
    else if (round == 2)
    {
        if (bsMode == BSMode::CONFIGURE)
        {
            if (evaluate(UseMiniTarget::YES))
            {
                writeTargetConfigCacheAtConfigureTime(true);
            }
        }

        if (bsMode == BSMode::BUILD && evaluate(UseMiniTarget::YES))
        {
            readConfigCacheAtBuildTime();
        }
        populateRequirementAndUsageRequirementDeps();
        // Needed to maintain ordering between different includes specification.
        reqIncSizeBeforePopulate = reqIncls.size();
        populateTransitiveProperties();

        buildCacheFilesDirPath = configureNode->filePath + slashc + name + slashc + "cf" + slashc;
        if (bsMode == BSMode::CONFIGURE)
        {
            create_directories(buildCacheFilesDirPath);
        }

        readBuildCacheFile(builder);
        // getCompileCommand will be later on called concurrently therefore need to set this before.
        setCompileCommand();
        compileCommandWithTool.setCommand((compiler.bTPath.*toPStr)() + " " + compileCommand);
        setSourceCompileCommandPrintFirstHalf();
        parseModuleSourceFiles(builder);
        populateResolveRequirePathDependencies();
        if (bsMode == BSMode::CONFIGURE)
        {
            create_directories(buildCacheFilesDirPath);
            if (evaluate(UseMiniTarget::YES))
            {
                writeTargetConfigCacheAtConfigureTime(false);
            }
        }
    }
}

void CppSourceTarget::writeTargetConfigCacheAtConfigureTime(bool before) const
{
    if (before)
    {
        using CppTargetConfigCache = Indices::CppTargetConfigCache;

        writeIncDirsAtConfigTime(reqIncls, targetConfigCache[0][CppTargetConfigCache::reqInclsArray]);
        writeIncDirsAtConfigTime(useReqIncls, targetConfigCache[0][CppTargetConfigCache::useReqInclsArray]);
        writeIncDirsAtConfigTime(reqHuDirs, targetConfigCache[0][CppTargetConfigCache::reqHUDirsArray]);
        writeIncDirsAtConfigTime(useReqHuDirs, targetConfigCache[0][CppTargetConfigCache::useReqHUDirsArray]);

        testVectorHasUniqueElementsIncrement(targetConfigCache[0][CppTargetConfigCache::sourceFiles].GetArray(),
                                             "srcFileDeps", 1);
        testVectorHasUniqueElementsIncrement(targetConfigCache[0][CppTargetConfigCache::moduleFiles].GetArray(),
                                             "modFileDeps", 2);
        testVectorHasUniqueElements(reqIncls, "reqIncls");
        testVectorHasUniqueElements(useReqIncls, "useReqIncls");
        testVectorHasUniqueElements(reqHuDirs, "reqHuDirs");
        testVectorHasUniqueElements(useReqHuDirs, "useReqHuDirs");
    }
}

void CppSourceTarget::readConfigCacheAtBuildTime()
{
    using CppTargetConfigCache = Indices::CppTargetConfigCache;

    // TODO
    // use-template

    PValue &reqInclCache = targetConfigCache[0][CppTargetConfigCache::reqInclsArray];
    PValue &useReqInclCache = targetConfigCache[0][CppTargetConfigCache::useReqInclsArray];
    PValue &reqHUDirCache = targetConfigCache[0][CppTargetConfigCache::reqHUDirsArray];
    PValue &useReqHUDirCache = targetConfigCache[0][CppTargetConfigCache::useReqHUDirsArray];
    PValue &sourceNodesCache = targetConfigCache[0][CppTargetConfigCache::sourceFiles];
    PValue &moduleNodesCache = targetConfigCache[0][CppTargetConfigCache::moduleFiles];

    constexpr uint8_t numOfElem = 3;
    reqIncls.reserve(reqInclCache.Size() / numOfElem);
    for (uint64_t i = 0; i < reqInclCache.Size(); i = i + numOfElem)
    {
        reqIncls.emplace_back(Node::getNodeFromPValue(reqInclCache[i], false), reqInclCache[i + 1].GetBool(),
                              reqInclCache[i + 2].GetBool());
    }

    useReqIncls.reserve(useReqInclCache.Size() / numOfElem);
    for (uint64_t i = 0; i < useReqInclCache.Size(); i = i + numOfElem)
    {
        useReqIncls.emplace_back(Node::getNodeFromPValue(useReqInclCache[i], false), useReqInclCache[i + 1].GetBool(),
                                 useReqInclCache[i + 2].GetBool());
    }

    reqHuDirs.reserve(reqHUDirCache.Size() / numOfElem);
    for (uint64_t i = 0; i < reqHUDirCache.Size(); i = i + numOfElem)
    {
        reqHuDirs.emplace_back(InclNode(Node::getNodeFromPValue(reqHUDirCache[i], false),
                                        reqHUDirCache[i + 1].GetBool(), reqHUDirCache[i + 2].GetBool()),
                               this);
    }

    useReqHuDirs.reserve(useReqHUDirCache.Size() / numOfElem);
    for (uint64_t i = 0; i < useReqHUDirCache.Size(); i = i + numOfElem)
    {
        useReqHuDirs.emplace_back(InclNode(Node::getNodeFromPValue(useReqHUDirCache[i], false),
                                           useReqHUDirCache[i + 1].GetBool(), useReqHUDirCache[i + 2].GetBool()),
                                  this);
    }

    srcFileDeps.reserve(sourceNodesCache.Size());
    for (PValue &pValue : sourceNodesCache.GetArray())
    {
        srcFileDeps.emplace_back(this, Node::getNodeFromPValue(pValue, true));
    }

    modFileDeps.reserve(moduleNodesCache.Size() / 2);
    for (uint64_t i = 0; i < moduleNodesCache.Size(); i = i + 2)
    {
        modFileDeps.emplace_back(this, Node::getNodeFromPValue(moduleNodesCache[i], true));
        modFileDeps[i / 2].isInterface = moduleNodesCache[i + 1].GetBool();
    }
}

pstring CppSourceTarget::getTarjanNodeName() const
{
    return "CppSourceTarget " + configureNode->filePath + slashc + name;
}

CppSourceTarget &CppSourceTarget::publicCompilerFlags(const pstring &compilerFlags)
{
    requirementCompilerFlags += compilerFlags;
    usageRequirementCompilerFlags += compilerFlags;
    return *this;
}

CppSourceTarget &CppSourceTarget::privateCompilerFlags(const pstring &compilerFlags)
{
    requirementCompilerFlags += compilerFlags;
    return *this;
}

CppSourceTarget &CppSourceTarget::interfaceCompilerFlags(const pstring &compilerFlags)
{
    usageRequirementCompilerFlags += compilerFlags;
    return *this;
}

CppSourceTarget &CppSourceTarget::publicCompileDefinition(const pstring &cddName, const pstring &cddValue)
{
    requirementCompileDefinitions.emplace(cddName, cddValue);
    usageRequirementCompileDefinitions.emplace(cddName, cddValue);
    return *this;
}

CppSourceTarget &CppSourceTarget::privateCompileDefinition(const pstring &cddName, const pstring &cddValue)
{
    requirementCompileDefinitions.emplace(cddName, cddValue);
    return *this;
}

CppSourceTarget &CppSourceTarget::interfaceCompileDefinition(const pstring &cddName, const pstring &cddValue)
{
    usageRequirementCompileDefinitions.emplace(cddName, cddValue);
    return *this;
}

void CppSourceTarget::parseRegexSourceDirs(bool assignToSourceNodes, const pstring &sourceDirectory, pstring regex,
                                           const bool recursive)
{
    if (evaluate(TreatModuleAsSource::YES))
    {
        assignToSourceNodes = true;
    }

    if (bsMode == BSMode::BUILD && useMiniTarget == UseMiniTarget::YES)
    {
        // Initialized in CppSourceTarget round 2
        return;
    }

    const SourceDirectory dir{sourceDirectory, std::move(regex), recursive};
    auto addNewFile = [&](const auto &k) {
        try
        {
            if (k.is_regular_file() && regex_match((k.path().filename().*toPStr)(), std::regex(dir.regex)))
            {
                Node *node = Node::getNodeFromNonNormalizedPath(k.path(), true);
                if (assignToSourceNodes)
                {
                    if (evaluate(UseMiniTarget::YES))
                    {
                        targetConfigCache[0][Indices::CppTargetConfigCache::sourceFiles].PushBack(node->getPValue(),
                                                                                                  ralloc);
                    }
                    else
                    {
                        actuallyAddSourceFile(srcFileDeps, node, this);
                    }
                }
                else
                {
                    if (evaluate(UseMiniTarget::YES))
                    {
                        targetConfigCache[0][Indices::CppTargetConfigCache::moduleFiles].PushBack(node->getPValue(),
                                                                                                  ralloc);
                        // isInterface is false
                        targetConfigCache[0][Indices::CppTargetConfigCache::moduleFiles].PushBack(PValue(false),
                                                                                                  ralloc);
                    }
                    else
                    {
                        actuallyAddModuleFile(modFileDeps, node, this);
                    }
                }
            }
        }
        catch (const std::regex_error &e)
        {
            printErrorMessage(fmt::format("regex_error : {}\nError happened while parsing regex {} of target{}\n",
                                          e.what(), dir.regex, name));
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

void CppSourceTarget::setCompileCommand()
{

    const CompilerFlags flags = getCompilerFlags();

    if (compiler.bTFamily == BTFamily::GCC)
    {
        // string str = cSourceTarget == CSourceTargetEnum::YES ? "-xc" : "-xc++";
        compileCommand +=
            flags.LANG + flags.OPTIONS + flags.OPTIONS_COMPILE + flags.OPTIONS_COMPILE_CPP + flags.DEFINES_COMPILE_CPP;
    }
    else if (compiler.bTFamily == BTFamily::MSVC)
    {
        const string str = cSourceTarget == CSourceTargetEnum::YES ? "-TC" : "-TP";
        compileCommand += str + " " + flags.CPP_FLAGS_COMPILE_CPP + flags.CPP_FLAGS_COMPILE + flags.OPTIONS_COMPILE +
                          flags.OPTIONS_COMPILE_CPP;
    }

    const pstring translateIncludeFlag =
        GET_FLAG_evaluate(AND(TranslateInclude::YES, BTFamily::MSVC), "/translateInclude ");
    compileCommand += translateIncludeFlag;

    auto getIncludeFlag = [this]() {
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            return "/I ";
        }
        return "-I ";
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
    // reqIncls can be made set, but this is not done to maintain specification order for include-dirs

    // I think ideally this should not be support this. A same header-file should not present in more than one
    // header-file.

    auto it = reqIncls.begin();
    for (unsigned short i = 0; i < reqIncSizeBeforePopulate; ++i)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(it->node->filePath) + " ");
        ++it;
    }

    set<pstring> includes;

    for (; it != reqIncls.end(); ++it)
    {
        includes.emplace(it->node->filePath);
    }

    for (const pstring &include : includes)
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
            getReducedPath((compiler.bTPath.make_preferred().*toPStr)(), ccpSettings.tool) + " ";
    }

    if (ccpSettings.infrastructureFlags)
    {
        const CompilerFlags flags = getCompilerFlags();
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

    auto getIncludeFlag = [this] {
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            return "/I ";
        }
        return "-I ";
    };

    for (const InclNode &include : reqIncls)
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

pstring &CppSourceTarget::getSourceCompileCommandPrintFirstHalf()
{
    if (sourceCompileCommandPrintFirstHalf.empty())
    {
        setSourceCompileCommandPrintFirstHalf();
    }
    return sourceCompileCommandPrintFirstHalf;
}

void CppSourceTarget::readBuildCacheFile(Builder &)
{
    // The search is not mutex-locked because no new value is added in preSort

    buildCacheIndex = pvalueIndexInSubArray(buildCache, PValue(ptoref(name)));

    if (buildCacheIndex == UINT64_MAX)
    {
        targetBuildCache = make_unique<PValue>(PValue(kArrayType));

        // Maybe store pointers to these in CppSourceTarget
        targetBuildCache->PushBack(PValue(kArrayType), cppAllocator);
        (*targetBuildCache)[Indices::CppTargetBuildCache::sourceFiles].Reserve(srcFileDeps.size(), cppAllocator);

        targetBuildCache->PushBack(PValue(kArrayType), cppAllocator);
        (*targetBuildCache)[Indices::CppTargetBuildCache::moduleFiles].Reserve(modFileDeps.size(), cppAllocator);

        // Header-units size can't be known as these are dynamically discovered.
        targetBuildCache->PushBack(PValue(kArrayType), cppAllocator);
    }
    else
    {
        // This copy is created becasue otherwise all access to the SMFile and SourceNode cache pointers in
        // targetBuildCache would be mutex-locked. Is this copying actually better than mutex-locking or both are same
        targetBuildCache =
            make_unique<PValue>(PValue(buildCache[buildCacheIndex][Indices::targetBuildCache], cppAllocator));

        PValue &sourceValue = (*targetBuildCache)[Indices::CppTargetBuildCache::sourceFiles];
        sourceValue.Reserve(sourceValue.Size() + srcFileDeps.size(), cppAllocator);

        PValue &moduleValue = (*targetBuildCache)[Indices::CppTargetBuildCache::moduleFiles];
        moduleValue.Reserve(moduleValue.Size() + modFileDeps.size(), cppAllocator);

        // Header-units size can't be known as these are dynamically discovered.
    }
}

void CppSourceTarget::resolveRequirePaths()
{
    for (const SMFile &smFile : modFileDeps)
    {
        using ModuleFiles = Indices::CppTargetBuildCache::ModuleFiles;

        for (PValue &require :
             (*smFile.sourceJson)[ModuleFiles::smRules][ModuleFiles::SmRules::requireArray].GetArray())
        {
            using SingleModuleDep = Indices::CppTargetBuildCache::ModuleFiles::SmRules::SingleModuleDep;

            // TODO
            // Change smFile->logicalName to PValue to efficiently compare it with Value PValue
            if (require[SingleModuleDep::logicalName] == PValue(ptoref(smFile.logicalName)))
            {
                printErrorMessageColor(fmt::format("In target\n{}\nModule\n{}\n can not depend on itself.\n",
                                                   getTarjanNodeName(), smFile.node->filePath),
                                       settings.pcSettings.toolErrorOutput);
                throw std::exception();
            }

            if (require[SingleModuleDep::isHeaderUnit].GetBool())
            {
                continue;
            }

            const SMFile *found = nullptr;

            // Even if found, we continue the search to ensure that no two files are providing the same module in
            // the module-files of the target and its dependencies
            if (auto it = requirePaths.find(pstring(require[SingleModuleDep::logicalName].GetString(),
                                                    require[SingleModuleDep::logicalName].GetStringLength()));
                it != requirePaths.end())
            {
                found = it->second;
            }

            for (CSourceTarget *cSourceTarget : requirementDeps)
            {
                if (cSourceTarget->getCSourceTargetType() == CSourceTargetType::CppSourceTarget)
                {
                    auto *cppSourceTarget = static_cast<CppSourceTarget *>(cSourceTarget);
                    if (auto it = cppSourceTarget->requirePaths.find(
                            pstring(require[SingleModuleDep::logicalName].GetString(),
                                    require[SingleModuleDep::logicalName].GetStringLength()));
                        it != cppSourceTarget->requirePaths.end())
                    {
                        if (found)
                        {
                            // Module was already found so error-out
                            printErrorMessageColor(
                                fmt::format("In target and its dependencies files:\n{}\nModule name:\n {}\n Is Being "
                                            "Provided By 2 "
                                            "different files:\n1){}\n2){}\n",
                                            getTarjanNodeName(),
                                            pstring(require[SingleModuleDep::logicalName].GetString(),
                                                    require[SingleModuleDep::logicalName].GetStringLength()),
                                            it->second->node->filePath, found->node->filePath),
                                settings.pcSettings.toolErrorOutput);
                            throw std::exception();
                        }
                        found = it->second;
                    }
                }
            }

            if (found)
            {
                const_cast<SMFile &>(smFile).realBTargets[0].addDependency(const_cast<SMFile &>(*found));
                if (!atomic_ref(smFile.fileStatus).load())
                {
                    if (require[SingleModuleDep::fullPath].GetString() != found->objectFileOutputFilePath->filePath)
                    {
                        atomic_ref(const_cast<SMFile &>(smFile).fileStatus).store(true);
                    }
                }
                const_cast<SMFile &>(smFile).pValueObjectFileMapping.emplace_back(&require,
                                                                                  found->objectFileOutputFilePath);
            }
            else
            {
                printErrorMessageColor(
                    fmt::format("No File in the target\n{}\n or in its dependencies provides this module {}.\n",
                                getTarjanNodeName(),
                                pstring(require[SingleModuleDep::logicalName].GetString(),
                                        require[SingleModuleDep::logicalName].GetStringLength())),
                    settings.pcSettings.toolErrorOutput);
                throw std::exception();
            }
        }
    }
}

void CppSourceTarget::populateSourceNodes()
{
    PValue &sourceFilesJson = (*targetBuildCache)[Indices::CppTargetBuildCache::sourceFiles];

    for (const SourceNode &sourceNodeConst : srcFileDeps)
    {
        auto &sourceNode = const_cast<SourceNode &>(sourceNodeConst);

        realBTargets[0].addDependency(sourceNode);

        const size_t fileIt = pvalueIndexInSubArray(sourceFilesJson, PValue(sourceNode.node->getPValue()));

        if (fileIt != UINT64_MAX)
        {
            sourceNode.sourceJson = &sourceFilesJson[fileIt];
        }
        else
        {
            sourceFilesJson.PushBack(PValue(kArrayType), sourceNode.sourceNodeAllocator);
            PValue &sourceJson = *(sourceFilesJson.End() - 1);
            sourceNode.sourceJson = &sourceJson;
            Node::emplaceNodeInPValue(sourceNode.node, sourceJson, sourceNode.sourceNodeAllocator);
            sourceJson.PushBack(PValue(kStringType), sourceNode.sourceNodeAllocator);
            sourceJson.PushBack(PValue(kArrayType), sourceNode.sourceNodeAllocator);
        }
    }
}

void CppSourceTarget::parseModuleSourceFiles(Builder &)
{
    PValue &moduleFilesJson = (*targetBuildCache)[Indices::CppTargetBuildCache::moduleFiles];

    for (const SMFile &smFileConst : modFileDeps)
    {
        auto &smFile = const_cast<SMFile &>(smFileConst);

        realBTargets[0].addDependency(smFile);
        resolveRequirePathBTarget.realBTargets[1].addDependency(smFile);
        adjustHeaderUnitsBTarget.realBTargets[1].addDependency(smFile);

        const size_t fileIt = pvalueIndexInSubArray(moduleFilesJson, PValue(smFile.node->getPValue()));

        if (fileIt != UINT64_MAX)
        {
            smFile.sourceJson = &moduleFilesJson[fileIt];
        }
        else
        {
            moduleFilesJson.PushBack(PValue(kArrayType), smFile.sourceNodeAllocator);
            PValue &moduleJson = *(moduleFilesJson.End() - 1);
            smFile.sourceJson = &moduleJson;
            Node::emplaceNodeInPValue(smFile.node, moduleJson, smFile.sourceNodeAllocator);
            moduleJson.PushBack(PValue(kStringType), smFile.sourceNodeAllocator);
            moduleJson.PushBack(PValue(kArrayType), smFile.sourceNodeAllocator);
            moduleJson.PushBack(PValue(kArrayType), smFile.sourceNodeAllocator);
            moduleJson.PushBack(PValue(kStringType), smFile.sourceNodeAllocator);
        }
    }
}

void CppSourceTarget::populateResolveRequirePathDependencies()
{
    for (CSourceTarget *target : requirementDeps)
    {
        if (target->getCSourceTargetType() == CSourceTargetType::CppSourceTarget)
        {
            if (const auto cppSourceTarget = static_cast<CppSourceTarget *>(target);
                !cppSourceTarget->modFileDeps.empty())
            {
                resolveRequirePathBTarget.realBTargets[1].addDependency(cppSourceTarget->resolveRequirePathBTarget);
            }
        }
    }
}

pstring CppSourceTarget::getInfrastructureFlags(const bool showIncludes) const
{
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        pstring str = GET_FLAG_evaluate(TargetType::LIBRARY_OBJECT, "-c", TargetType::PREPROCESS, "-P");
        str += " /nologo ";
        if (showIncludes)
        {
            str += "/showIncludes ";
        }
        return str;
    }

    if (compiler.bTFamily == BTFamily::GCC)
    {
        // Will like to use -MD but not using it currently because sometimes it
        // prints 2 header deps in one line and no space in them so no way of
        // knowing whether this is a space in path or 2 different headers. Which
        // then breaks when last_write_time is checked for that path.
        return GET_FLAG_evaluate(TargetType::LIBRARY_OBJECT, "-c", TargetType::PREPROCESS, "-E") + " -MMD";
    }
    return "";
}

pstring CppSourceTarget::getCompileCommandPrintSecondPart(const SourceNode &sourceNode) const
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;

    pstring command;
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
        command += compiler.bTFamily == BTFamily::MSVC ? evaluate(TargetType::LIBRARY_OBJECT) ? "/Fo" : "/Fi" : "-o ";
    }
    if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(sourceNode.objectFileOutputFilePath->filePath, ccpSettings.objectFile) + " ";
    }
    return command;
}

pstring CppSourceTarget::getCompileCommandPrintSecondPartSMRule(const SMFile &smFile) const
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;

    pstring command;

    if (ccpSettings.sourceFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(smFile.node->filePath, ccpSettings.sourceFile) + " ";
    }
    if (ccpSettings.infrastructureFlags)
    {
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            const pstring translateIncludeFlag = GET_FLAG_evaluate(TranslateInclude::YES, "/translateInclude ");
            command += translateIncludeFlag + " /nologo /showIncludes /scanDependencies ";
        }
    }
    if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
    {
        command +=
            getReducedPath(buildCacheFilesDirPath + (path(smFile.node->filePath).filename().*toPStr)() + ".smrules",
                           ccpSettings.objectFile) +
            " ";
    }

    return command;
}

PostCompile CppSourceTarget::CompileSMFile(const SMFile &smFile)
{
    targetCacheChanged.store(true);
    pstring finalCompileCommand = compileCommand;

    for (const SMFile *smFileLocal : smFile.allSMFileDependenciesRoundZero)
    {
        finalCompileCommand += smFileLocal->getRequireFlag(smFile);
    }
    finalCompileCommand += getInfrastructureFlags(false) + " " + addQuotes(smFile.node->filePath) + " ";

    finalCompileCommand += smFile.getFlag(buildCacheFilesDirPath + (path(smFile.node->filePath).filename().*toPStr)());

    return PostCompile{*this,
                       compiler,
                       finalCompileCommand,
                       getSourceCompileCommandPrintFirstHalf() + smFile.getModuleCompileCommandPrintLastHalf(),
                       buildCacheFilesDirPath,
                       (path(smFile.node->filePath).filename().*toPStr)(),
                       settings.ccpSettings.outputAndErrorFiles};
}

pstring CppSourceTarget::getExtension() const
{
    return GET_FLAG_evaluate(TargetType::PREPROCESS, ".ii", TargetType::LIBRARY_OBJECT, ".o");
}

mutex cppSourceTargetDotCpp_TempMutex;

PostCompile CppSourceTarget::updateSourceNodeBTarget(const SourceNode &sourceNode)
{
    targetCacheChanged.store(true);

    const pstring compileFileName = (path(sourceNode.node->filePath).filename().*toPStr)();

    pstring finalCompileCommand = compileCommand + " ";

    finalCompileCommand += getInfrastructureFlags(true) + " " + addQuotes(sourceNode.node->filePath) + " ";
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        finalCompileCommand += (evaluate(TargetType::LIBRARY_OBJECT) ? "/Fo" : "/Fi") +
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

PostCompile CppSourceTarget::GenerateSMRulesFile(const SMFile &smFile, const bool printOnlyOnError)
{
    targetCacheChanged.store(true);
    pstring finalCompileCommand = compileCommand + addQuotes(smFile.node->filePath) + " ";

    if (compiler.bTFamily == BTFamily::MSVC)
    {
        finalCompileCommand +=
            " /nologo /showIncludes /scanDependencies " +
            addQuotes(buildCacheFilesDirPath + (path(smFile.node->filePath).filename().*toPStr)() + ".smrules") + " ";

        return printOnlyOnError
                   ? PostCompile(*this, compiler, finalCompileCommand, "", buildCacheFilesDirPath,
                                 (path(smFile.node->filePath).filename().*toPStr)() + ".smrules",
                                 settings.ccpSettings.outputAndErrorFiles)
                   : PostCompile(
                         *this, compiler, finalCompileCommand,
                         getSourceCompileCommandPrintFirstHalf() + getCompileCommandPrintSecondPartSMRule(smFile),
                         buildCacheFilesDirPath, (path(smFile.node->filePath).filename().*toPStr)() + ".smrules",
                         settings.ccpSettings.outputAndErrorFiles);
    }
    if (compiler.bTFamily == BTFamily::GCC)
    {
        // clang flags. gcc not yet supported.
        finalCompileCommand =
            "-format=p1689 -- " + compiler.bTPath.string() + " " + finalCompileCommand + " -c -MMD -MF ";
        finalCompileCommand +=
            addQuotes(buildCacheFilesDirPath + (path(smFile.node->filePath).filename().*toPStr)() + ".d");

        return printOnlyOnError
                   ? PostCompile(*this, scanner, finalCompileCommand, "", buildCacheFilesDirPath,
                                 (path(smFile.node->filePath).filename().*toPStr)() + ".smrules",
                                 settings.ccpSettings.outputAndErrorFiles)
                   : PostCompile(
                         *this, scanner, finalCompileCommand,
                         getSourceCompileCommandPrintFirstHalf() + getCompileCommandPrintSecondPartSMRule(smFile),
                         buildCacheFilesDirPath, (path(smFile.node->filePath).filename().*toPStr)() + ".smrules",
                         settings.ccpSettings.outputAndErrorFiles);
    }
}

void CppSourceTarget::saveBuildCache(const bool round)
{
    lock_guard lk{buildCacheMutex};

    if (buildCacheIndex == UINT64_MAX)
    {
        buildCache.PushBack(PValue(kArrayType), ralloc);
        PValue &t = *(buildCache.End() - 1);
        t.PushBack(ptoref(name), ralloc);
        t.PushBack(PValue(*targetBuildCache, ralloc), ralloc);
        buildCacheIndex = buildCache.Size() - 1;
    }
    else
    {
        buildCache[buildCacheIndex][1].CopyFrom(*targetBuildCache, ralloc);
    }

    if (round)
    {
        writeBuildCacheUnlocked();
    }
    else
    {
        if (archiving)
        {
            if (realBTargets[0].exitStatus == EXIT_SUCCESS)
            {
                archived = true;
            }
        }
        writeBuildCacheUnlocked();
    }
}

bool operator<(const CppSourceTarget &lhs, const CppSourceTarget &rhs)
{
    return lhs.name < rhs.name;
}