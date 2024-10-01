
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
    auto getPStringFromArchitectureEnum = [](const Arch arch) -> pstring {
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
        printErrorMessage("conversion from json pstring literal to enum class Arch failed\n");
        throw std::exception();
    }
}

void to_json(Json &j, const AddressModel &am)
{
    auto getPStringFromArchitectureEnum = [](const AddressModel am) -> pstring {
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
        printErrorMessage("conversion from json pstring literal to enum class AM failed\n");
        throw std::exception();
    }
}

TemplateDepth::TemplateDepth(const unsigned long long templateDepth_) : templateDepth(templateDepth_)
{
}

Define::Define(pstring name_, pstring value_) : name{std::move(name_)}, value{std::move(value_)}
{
}

void to_json(Json &j, const Define &cd)
{
    j[JConsts::name] = cd.name;
    j[JConsts::value] = cd.value;
}

void from_json(const Json &j, Define &cd)
{
    cd.name = j.at(JConsts::name).get<pstring>();
    cd.value = j.at(JConsts::value).get<pstring>();
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

pstring getActualNameFromTargetName(const TargetType bTargetType, const OS osLocal, const pstring &targetName)
{
    if (bTargetType == TargetType::EXECUTABLE)
    {
        return targetName + (osLocal == OS::NT ? ".exe" : "");
    }
    if (bTargetType == TargetType::LIBRARY_STATIC || bTargetType == TargetType::PLIBRARY_STATIC)
    {
        pstring actualName = osLocal == OS::NT ? "" : "lib";
        actualName += targetName;
        actualName += osLocal == OS::NT ? ".lib" : ".a";
        return actualName;
    }
    if (bTargetType == TargetType::LIBRARY_SHARED || bTargetType == TargetType::PLIBRARY_SHARED)
    {
        pstring actualName = osLocal == OS::NT ? "" : "lib";
        actualName += targetName;
        actualName += osLocal == OS::NT ? ".dll" : ".so";
        return actualName;
    }
    printErrorMessage("Other Targets Are Not Supported Yet.\n");
    throw std::exception();
}

pstring getTargetNameFromActualName(const TargetType bTargetType, const OS osLocal, const pstring &actualName)
{
    if (bTargetType == TargetType::EXECUTABLE)
    {
        return osLocal == OS::NT ? actualName + ".exe" : actualName;
    }
    if (bTargetType == TargetType::LIBRARY_STATIC || bTargetType == TargetType::PLIBRARY_STATIC)
    {
        pstring libName = actualName;
        // Removes lib from libName.a
        libName = osLocal == OS::NT ? actualName : libName.erase(0, 3);
        // Removes .a from libName.a or .lib from Name.lib
        const unsigned short eraseCount = osLocal == OS::NT ? 4 : 2;
        libName = libName.erase(libName.find('.'), eraseCount);
        return libName;
    }
    if (bTargetType == TargetType::LIBRARY_SHARED || bTargetType == TargetType::PLIBRARY_SHARED)
    {
        pstring libName = actualName;
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

pstring getSlashedExecutableName(const pstring &name)
{
    return os == OS::NT ? name + ".exe" : "./" + name;
}

PrebuiltBasicFeatures::PrebuiltBasicFeatures()
{
    if (cache.isLinkerInToolsArray)
    {
        VSTools &vsTools = toolsCache.vsTools[cache.selectedLinkerArrayIndex];
        if (bsMode == BSMode::BUILD && useMiniTarget == UseMiniTarget::YES)
        {
            // Initialized in LinkOrArchiveTarget round 2
            return;
        }
        for (const pstring &str : vsTools.libraryDirectories)
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
    configurationType = cache.configurationType;
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
    libraryType = cache.libraryType;
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

CppCompilerFeatures::CppCompilerFeatures()
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
    configurationType = cache.configurationType;
    setConfigType(configurationType);
    if (cache.isCompilerInToolsArray)
    {
        if constexpr (os == OS::NT)
        {
            setCompilerFromVSTools(toolsCache.vsTools[cache.selectedCompilerArrayIndex]);
        }
        else
        {
            setCompilerFromLinuxTools(toolsCache.linuxTools[cache.selectedCompilerArrayIndex]);
        }
    }
    else
    {
        compiler = toolsCache.compilers[cache.selectedCompilerArrayIndex];
    }

    scanner.bTPath = compiler.bTPath.parent_path() / "clang-scan-deps";
}

// Use getNodeFromNormalizedPath instead

void CppCompilerFeatures::setCompilerFromVSTools(const VSTools &vsTools)
{
    compiler = vsTools.compiler;

    if (bsMode == BSMode::BUILD && useMiniTarget == UseMiniTarget::YES)
    {
        // Initialized in CppSourceTarget round 2
        return;
    }
    for (const pstring &str : vsTools.includeDirectories)
    {
        actuallyAddInclude(reqIncls, str, true, true);
    }
}

void CppCompilerFeatures::setCompilerFromLinuxTools(const LinuxTools &linuxTools)
{
    compiler = linuxTools.compiler;
    if (bsMode == BSMode::BUILD && useMiniTarget == UseMiniTarget::YES)
    {
        // Initialized in CppSourceTarget round 2
        return;
    }
    for (const pstring &str : linuxTools.includeDirectories)
    {
        actuallyAddInclude(reqIncls, str, true, true);
    }
}

void CppCompilerFeatures::setConfigType(const ConfigType configType)
{
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

bool CppCompilerFeatures::actuallyAddInclude(vector<InclNode> &inclNodes, const pstring &include, bool isStandard,
                                             bool ignoreHeaderDeps)
{
    Node *node = Node::getNodeFromNonNormalizedPath(include, false);
    bool found = false;
    for (const InclNode &inclNode : inclNodes)
    {
        if (inclNode.node->myId == node->myId)
        {
            found = true;
            printErrorMessage(fmt::format("Include {} is already added.\n", node->filePath));
            break;
        }
    }
    if (!found)
    {
        inclNodes.emplace_back(node, isStandard, ignoreHeaderDeps);
        return true;
    }
    return false;
}

bool CppCompilerFeatures::actuallyAddInclude(CppSourceTarget *target, vector<InclNodeTargetMap> &inclNodes,
                                             const pstring &include, bool isStandard, bool ignoreHeaderDeps)
{
    Node *node = Node::getNodeFromNonNormalizedPath(include, false);
    bool found = false;
    for (const InclNodeTargetMap &inclNode : inclNodes)
    {
        if (inclNode.inclNode.node->myId == node->myId)
        {
            found = true;
            printErrorMessage(
                fmt::format("Header-unit include {} already exists in target {}.\n", node->filePath, target->name));
            break;
        }
    }
    if (!found)
    {
        inclNodes.emplace_back(InclNode(node, isStandard), target);
        return true;
    }
    return false;
}