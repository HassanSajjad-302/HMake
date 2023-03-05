#include "Features.hpp"
#include "BuildSystemFunctions.hpp"
#include "Cache.hpp"
#include "CppSourceTarget.hpp"
#include "JConsts.hpp"
#include "ToolsCache.hpp"
#include "fmt/format.h"
#include "nlohmann/json.hpp"
using Json = nlohmann::ordered_json;
using fmt::print;

void to_json(Json &j, const Arch &arch)
{
    auto getStringFromArchitectureEnum = [](Arch arch) -> string {
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
    j = getStringFromArchitectureEnum(arch);
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
        print(stderr, "conversion from json string literal to enum class Arch failed");
        exit(EXIT_FAILURE);
    }
}

void to_json(Json &j, const AddressModel &am)
{
    auto getStringFromArchitectureEnum = [](AddressModel am) -> string {
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
    j = getStringFromArchitectureEnum(am);
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
        print(stderr, "conversion from json string literal to enum class AM failed");
        exit(EXIT_FAILURE);
    }
}

TemplateDepth::TemplateDepth(unsigned long long templateDepth_) : templateDepth(templateDepth_)
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

string getActualNameFromTargetName(TargetType bTargetType, const OS osLocal, const string &targetName)
{
    if (bTargetType == TargetType::EXECUTABLE)
    {
        return targetName + (osLocal == OS::NT ? ".exe" : "");
    }
    else if (bTargetType == TargetType::LIBRARY_STATIC || bTargetType == TargetType::PLIBRARY_STATIC)
    {
        string actualName;
        actualName = osLocal == OS::NT ? "" : "lib";
        actualName += targetName;
        actualName += osLocal == OS::NT ? ".lib" : ".a";
        return actualName;
    }
    else if (bTargetType == TargetType::LIBRARY_SHARED || bTargetType == TargetType::PLIBRARY_SHARED)
    {
        string actualName;
        actualName = osLocal == OS::NT ? "" : "lib";
        actualName += targetName;
        actualName += osLocal == OS::NT ? ".dll" : ".so";
        return actualName;
    }
    print(stderr, "Other Targets Are Not Supported Yet.\n");
    exit(EXIT_FAILURE);
}

string getTargetNameFromActualName(TargetType bTargetType, const OS osLocal, const string &actualName)
{
    if (bTargetType == TargetType::EXECUTABLE)
    {
        return osLocal == OS::NT ? actualName + ".exe" : actualName;
    }
    else if (bTargetType == TargetType::LIBRARY_STATIC || bTargetType == TargetType::PLIBRARY_STATIC)
    {
        string libName = actualName;
        // Removes lib from libName.a
        libName = osLocal == OS::NT ? actualName : libName.erase(0, 3);
        // Removes .a from libName.a or .lib from Name.lib
        unsigned short eraseCount = osLocal == OS::NT ? 4 : 2;
        libName = libName.erase(libName.find('.'), eraseCount);
        return libName;
    }
    else if (bTargetType == TargetType::LIBRARY_SHARED || bTargetType == TargetType::PLIBRARY_SHARED)
    {
        string libName = actualName;
        // Removes lib from libName.so
        libName = osLocal == OS::NT ? actualName : libName.erase(0, 3);
        // Removes .so from libName.so or .dll from Name.dll
        unsigned short eraseCount = osLocal == OS::NT ? 4 : 3;
        libName = libName.erase(libName.find('.'), eraseCount);
        return libName;
    }
    print(stderr, "Other Targets Are Not Supported Yet.\n");
    exit(EXIT_FAILURE);
}

string getSlashedExecutableName(const string &name)
{
    return os == OS::NT ? (name + ".exe") : ("./" + name);
}

CommonFeatures::CommonFeatures()
{
    // TODO
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
}

void CommonFeatures::setConfigType(ConfigType configType)
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

LinkerFeatures::LinkerFeatures()
{
    if (cache.isLinkerInVSToolsArray)
    {
        setLinkerFromVSTools(toolsCache.vsTools[cache.selectedLinkerArrayIndex]);
    }
    else
    {
        linker = toolsCache.linkers[cache.selectedLinkerArrayIndex];
    }
    if (cache.isArchiverInVSToolsArray)
    {
        archiver = toolsCache.vsTools[cache.selectedArchiverArrayIndex].archiver;
    }
    else
    {
        archiver = toolsCache.archivers[cache.selectedArchiverArrayIndex];
    }
    libraryType = cache.libraryType;
}

void LinkerFeatures::setLinkerFromVSTools(struct VSTools &vsTools)
{
    linker = vsTools.linker;
    standardLibraryDirectories.clear();
    for (const string &str : vsTools.libraryDirectories)
    {
        standardLibraryDirectories.emplace(Node::getNodeFromString(str, false));
    }
}

CompilerFeatures::CompilerFeatures()
{
    if (cache.isCompilerInVSToolsArray)
    {
        setCompilerFromVSTools(toolsCache.vsTools[cache.selectedCompilerArrayIndex]);
    }
    else
    {
        compiler = toolsCache.compilers[cache.selectedCompilerArrayIndex];
    }
}

void CompilerFeatures::setCompilerFromVSTools(VSTools &vsTools)
{
    compiler = vsTools.compiler;
    standardIncludes.clear();
    for (const string &str : vsTools.includeDirectories)
    {
        Node *node =
            const_cast<Node *>(standardIncludes.emplace(Node::getNodeFromString(str, false)).first.operator*());
        node->ignoreIncludes = true;
    }
}

void CompilerFeatures::setConfigType(ConfigType configurationType)
{
    // Value is set according to Comment about variant in variant-feature.jam. But, actually in builtin.jam where
    // variant is set runtime-debugging is not set though. Here it is set, however.
    if (configurationType == ConfigType::DEBUG)
    {
        optimization = Optimization::OFF;
        inlining = Inlining::OFF;
    }
    else if (configurationType == ConfigType::RELEASE)
    {
        optimization = Optimization::SPEED;
        inlining = Inlining::FULL;
    }
}