#ifndef HMAKE_CPPSOURCETARGET_HPP
#define HMAKE_CPPSOURCETARGET_HPP
#ifdef USE_HEADER_UNITS
import "BuildTools.hpp";
import "Prebuilt.hpp";
import "Features.hpp";
import "FeaturesConvenienceFunctions.hpp";
import "JConsts.hpp";
import "PostBasic.hpp";
import "ToolsCache.hpp";
import <concepts>;
import <set>;
import <string>;
#else
#include "BuildTools.hpp"
#include "CSourceTarget.hpp"
#include "Features.hpp"
#include "FeaturesConvenienceFunctions.hpp"
#include "JConsts.hpp"
#include "PostBasic.hpp"
#include "ToolsCache.hpp"
#include <concepts>
#include <set>
#include <string>
#endif

using std::set, std::string, std::same_as;

struct SourceDirectory
{
    const Node *sourceDirectory;
    string regex;
    bool recursive;
    SourceDirectory() = default;
    SourceDirectory(const string &sourceDirectory_, string regex_, bool recursive_ = false);
};
void to_json(Json &j, const SourceDirectory &sourceDirectory);
bool operator<(const SourceDirectory &lhs, const SourceDirectory &rhs);

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

inline mutex modulescopedata_smFiles;
inline mutex modulescopedata_headerUnits;
inline mutex modulescopedata_requirePaths;
struct ModuleScopeData
{
    // Written mutex locked in round 1 preSort.
    set<SMFile *> smFiles;

    // Written mutex locked in round 1 updateBTarget
    set<SMFile, CompareSourceNode> headerUnits;

    // Which header unit directory come from which target
    map<const InclNode *, CppSourceTarget *> huDirTarget;

    // Written mutex locked in round 1 updateBTarget.
    // Which require is provided by which SMFile
    map<string, SMFile *> requirePaths;

    set<CppSourceTarget *> targets;
    unsigned int totalSMRuleFileCount = 0;
};

class CppSourceTarget : public CppCompilerFeatures,
                        public CTarget,
                        public FeatureConvenienceFunctions<CppSourceTarget>,
                        public CSourceTarget
{
  public:
    using BaseType = CSourceTarget;
    // Written mutex locked in round 1 updateBTarget
    mutex headerUnitsMutex;
    Json targetBuildCache;
    TargetType compileTargetType;
    CppSourceTarget *moduleScope = nullptr;
    ModuleScopeData *moduleScopeData = nullptr;
    inline static map<const CppSourceTarget *, ModuleScopeData> moduleScopes;
    friend struct PostCompile;
    // Parsed Info Not Changed Once Read
    string targetFilePath;
    string buildCacheFilesDirPath;

    // Some State Variables
    // Compile Command excluding source-file or source-files(in case of module) that is also stored in the cache.
    string compileCommand;
    string sourceCompileCommandPrintFirstHalf;

    set<SourceNode, CompareSourceNode> sourceFileDependencies;
    // Comparator used is same as for SourceNode
    set<SMFile, CompareSourceNode> moduleSourceFileDependencies;
    // Set to true if a source or smrule is updated so that latest cache could be stored.
    std::atomic<bool> targetCacheChanged = false;
    void setCpuType();
    bool isCpuTypeG7();

    set<const SMFile *> headerUnits;

    string &getCompileCommand();
    void setCompileCommand();
    void setSourceCompileCommandPrintFirstHalf();
    inline string &getSourceCompileCommandPrintFirstHalf();

    void readBuildCacheFile(Builder &);
    void resolveRequirePaths();
    void populateSourceNodes();
    void parseModuleSourceFiles(Builder &builder);
    string getInfrastructureFlags(bool showIncludes);
    string getCompileCommandPrintSecondPart(const SourceNode &sourceNode);
    string getCompileCommandPrintSecondPartSMRule(const SMFile &smFile);
    PostCompile CompileSMFile(SMFile &smFile);
    string getExtension();
    PostCompile updateSourceNodeBTarget(SourceNode &sourceNode);

    PostCompile GenerateSMRulesFile(const SMFile &smFile, bool printOnlyOnError);
    void saveBuildCache(bool round);

    set<SourceDirectory> regexSourceDirs;
    set<SourceDirectory> regexModuleDirs;
    // requirementIncludes size before populateTransitiveProperties function is called
    unsigned short reqIncSizeBeforePopulate = 0;

    bool archiving = false;
    bool archived = false;

    void preSort(Builder &builder, unsigned short round) override;
    void updateBTarget(Builder &builder, unsigned short round) override;
    /*    void setJson() override;
        void writeJsonFile() override;*/
    string getTarjanNodeName() const override;
    BTarget *getBTarget() override;
    C_Target *get_CAPITarget(BSMode bsMode) override;
    CompilerFlags getCompilerFlags();
    // TODO
  public:
    CppSourceTarget(string name_, TargetType targetType);
    CppSourceTarget(string name_, TargetType targetType, CTarget &other, bool hasFile = true);

    void getObjectFiles(vector<const ObjectFile *> *objectFiles,
                        LinkOrArchiveTarget *linkOrArchiveTarget) const override;
    void populateTransitiveProperties();
    void registerHUInclNode(const InclNode &inclNode);

    CppSourceTarget &setModuleScope(CppSourceTarget *moduleScope_);
    CppSourceTarget &setModuleScope();

    CppSourceTarget &assignStandardIncludesToHUIncludes();
    // TODO
    // Also provide function overload for functions like PUBLIC_INCLUDES here and in CPT
    template <typename... U> CppSourceTarget &PUBLIC_INCLUDES(const string &include, U... includeDirectoryString);
    template <typename... U> CppSourceTarget &PRIVATE_INCLUDES(const string &include, U... includeDirectoryString);
    template <typename... U> CppSourceTarget &INTERFACE_INCLUDES(const string &include, U... includeDirectoryString);
    template <typename... U> CppSourceTarget &PUBLIC_HU_INCLUDES(const string &include, U... includeDirectoryString);
    template <typename... U> CppSourceTarget &PRIVATE_HU_INCLUDES(const string &include, U... includeDirectoryString);
    template <typename... U> CppSourceTarget &HU_DIRECTORIES(const string &include, U... includeDirectoryString);
    CppSourceTarget &PUBLIC_COMPILER_FLAGS(const string &compilerFlags);
    CppSourceTarget &PRIVATE_COMPILER_FLAGS(const string &compilerFlags);
    CppSourceTarget &INTERFACE_COMPILER_FLAGS(const string &compilerFlags);
    CppSourceTarget &PUBLIC_COMPILE_DEFINITION(const string &cddName, const string &cddValue = "");
    CppSourceTarget &PRIVATE_COMPILE_DEFINITION(const string &cddName, const string &cddValue = "");
    CppSourceTarget &INTERFACE_COMPILE_DEFINITION(const string &cddName, const string &cddValue = "");
    template <typename... U> CppSourceTarget &SOURCE_FILES(const string &srcFile, U... sourceFileString);
    template <typename... U> CppSourceTarget &MODULE_FILES(const string &modFile, U... moduleFileString);
    void parseRegexSourceDirs(bool assignToSourceNodes, bool recursive, const SourceDirectory &dir);
    template <typename... U> CppSourceTarget &SOURCE_DIRECTORIES(const string &sourceDirectory, U... directories);
    template <typename... U> CppSourceTarget &MODULE_DIRECTORIES(const string &moduleDirectory, U... directories);
    template <typename... U>
    CppSourceTarget &SOURCE_DIRECTORIES_RG(const string &sourceDirectory, const string &regex, U... directories);
    template <typename... U>
    CppSourceTarget &MODULE_DIRECTORIES_RG(const string &moduleDirectory, const string &regex, U... directories);
    template <typename... U> CppSourceTarget &R_SOURCE_DIRECTORIES(const string &sourceDirectory, U... directories);
    template <typename... U> CppSourceTarget &R_MODULE_DIRECTORIES(const string &moduleDirectory, U... directories);
    template <typename... U>
    CppSourceTarget &R_SOURCE_DIRECTORIES_RG(const string &sourceDirectory, const string &regex, U... directories);
    template <typename... U>
    CppSourceTarget &R_MODULE_DIRECTORIES_RG(const string &moduleDirectory, const string &regex, U... directories);
    //
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Property>
    CppSourceTarget &ASSIGN(T property, Property... properties);
    template <typename T> bool EVALUATE(T property) const;
    template <Dependency dependency = Dependency::PRIVATE, typename T> void assignCommonFeature(T property);
}; // class Target
bool operator<(const CppSourceTarget &lhs, const CppSourceTarget &rhs);

template <typename... U>
CppSourceTarget &CppSourceTarget::PUBLIC_INCLUDES(const string &include, U... includeDirectoryString)
{
    Node *node = Node::getNodeFromString(include, false);
    InclNode::emplaceInList(requirementIncludes, node);
    InclNode::emplaceInList(usageRequirementIncludes, node);
    if constexpr (sizeof...(includeDirectoryString))
    {
        return PUBLIC_INCLUDES(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::PRIVATE_INCLUDES(const string &include, U... includeDirectoryString)
{
    InclNode::emplaceInList(requirementIncludes, Node::getNodeFromString(include, false));
    if constexpr (sizeof...(includeDirectoryString))
    {
        return PRIVATE_INCLUDES(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::INTERFACE_INCLUDES(const string &include, U... includeDirectoryString)
{
    InclNode::emplaceInList(usageRequirementIncludes, Node::getNodeFromString(include, false));
    if constexpr (sizeof...(includeDirectoryString))
    {
        return INTERFACE_INCLUDES(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::PUBLIC_HU_INCLUDES(const string &include, U... includeDirectoryString)
{
    Node *node = Node::getNodeFromString(include, false);
    InclNode::emplaceInList(usageRequirementIncludes, node);
    if (bool added = InclNode::emplaceInList(requirementIncludes, node); added)
    {
        registerHUInclNode(requirementIncludes.back());
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return PUBLIC_HU_INCLUDES(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::PRIVATE_HU_INCLUDES(const string &include, U... includeDirectoryString)
{
    if (bool added = InclNode::emplaceInList(requirementIncludes, Node::getNodeFromString(include, false)); added)
    {
        registerHUInclNode(requirementIncludes.back());
    }
    if constexpr (sizeof...(includeDirectoryString))
    {
        return PRIVATE_HU_INCLUDES(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::HU_DIRECTORIES(const string &include, U... includeDirectoryString)
{
    registerHUInclNode(targets<InclNode>.emplace(Node::getNodeFromString(include, false)).first.operator*());

    if constexpr (sizeof...(includeDirectoryString))
    {
        return HU_DIRECTORIES(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppSourceTarget &CppSourceTarget::SOURCE_FILES(const string &srcFile, U... sourceFileString)
{
    sourceFileDependencies.emplace(this, const_cast<Node *>(Node::getNodeFromString(srcFile, true)));
    if constexpr (sizeof...(sourceFileString))
    {
        return SOURCE_FILES(sourceFileString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppSourceTarget &CppSourceTarget::MODULE_FILES(const string &modFile, U... moduleFileString)
{
    if (EVALUATE(TreatModuleAsSource::YES))
    {
        return SOURCE_FILES(modFile, moduleFileString...);
    }
    else
    {
        moduleSourceFileDependencies.emplace(this, const_cast<Node *>(Node::getNodeFromString(modFile, true)));
        if constexpr (sizeof...(moduleFileString))
        {
            return MODULE_FILES(moduleFileString...);
        }
        else
        {
            return *this;
        }
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::SOURCE_DIRECTORIES(const string &sourceDirectory, U... directories)
{
    if (const auto &[pos, Ok] = regexSourceDirs.emplace(sourceDirectory, ".*"); Ok)
    {
        parseRegexSourceDirs(true, false, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return SOURCE_DIRECTORIES(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::MODULE_DIRECTORIES(const string &moduleDirectory, U... directories)
{
    if (const auto &[pos, Ok] = regexModuleDirs.emplace(moduleDirectory, ".*"); Ok)
    {
        parseRegexSourceDirs(false, false, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return MODULE_DIRECTORIES(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::SOURCE_DIRECTORIES_RG(const string &sourceDirectory, const string &regex,
                                                        U... directories)
{
    if (const auto &[pos, Ok] = regexSourceDirs.emplace(sourceDirectory, regex); Ok)
    {
        parseRegexSourceDirs(true, false, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return SOURCE_DIRECTORIES_RG(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::MODULE_DIRECTORIES_RG(const string &moduleDirectory, const string &regex,
                                                        U... directories)
{
    if (const auto &[pos, Ok] = regexModuleDirs.emplace(moduleDirectory, regex); Ok)
    {
        parseRegexSourceDirs(false, false, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return MODULE_DIRECTORIES_RG(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::R_SOURCE_DIRECTORIES(const string &sourceDirectory, U... directories)
{
    if (const auto &[pos, Ok] = regexSourceDirs.emplace(sourceDirectory, ".*", true); Ok)
    {
        parseRegexSourceDirs(true, true, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return R_SOURCE_DIRECTORIES(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::R_MODULE_DIRECTORIES(const string &moduleDirectory, U... directories)
{
    if (const auto &[pos, Ok] = regexModuleDirs.emplace(moduleDirectory, ".*", true); Ok)
    {
        parseRegexSourceDirs(false, true, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return R_MODULE_DIRECTORIES(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::R_SOURCE_DIRECTORIES_RG(const string &sourceDirectory, const string &regex,
                                                          U... directories)
{
    if (const auto &[pos, Ok] = regexSourceDirs.emplace(sourceDirectory, regex, true); Ok)
    {
        parseRegexSourceDirs(true, true, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return R_SOURCE_DIRECTORIES_RG(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::R_MODULE_DIRECTORIES_RG(const string &moduleDirectory, const string &regex,
                                                          U... directories)
{
    if (const auto &[pos, Ok] = regexModuleDirs.emplace(moduleDirectory, regex, true); Ok)
    {
        parseRegexSourceDirs(false, true, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return R_MODULE_DIRECTORIES_RG(directories...);
    }
    return *this;
}

template <Dependency dependency, typename T, typename... Property>
CppSourceTarget &CppSourceTarget::ASSIGN(T property, Property... properties)
{
    if constexpr (std::is_same_v<decltype(property), Compiler>)
    {
        compiler = property;
    }
    else if constexpr (std::is_same_v<decltype(property), BTFamily>)
    {
        compiler.bTFamily = property;
    }
    else if constexpr (std::is_same_v<decltype(property), path>)
    {
        compiler.bTPath = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxFlags>)
    {
        if constexpr (dependency == Dependency::PRIVATE)
        {
            requirementCompilerFlags += property;
        }
        else if constexpr (dependency == Dependency::INTERFACE)
        {
            usageRequirementCompilerFlags += property;
        }
        else
        {
            requirementCompilerFlags += property;
            usageRequirementCompilerFlags += property;
        }
    }
    else if constexpr (std::is_same_v<decltype(property), Define>)
    {
        if constexpr (dependency == Dependency::PRIVATE)
        {
            requirementCompileDefinitions.emplace(property);
        }
        else if constexpr (dependency == Dependency::INTERFACE)
        {
            usageRequirementCompileDefinitions.emplace(property);
        }
        else
        {
            requirementCompileDefinitions.emplace(property);
            usageRequirementCompileDefinitions.emplace(property);
        }
    }
    else if constexpr (std::is_same_v<decltype(property), Threading>)
    {
        threading = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTD>)
    {
        cxxStd = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CxxSTDDialect>)
    {
        cxxStdDialect = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Optimization>)
    {
        optimization = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Inlining>)
    {
        inlining = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Warnings>)
    {
        warnings = property;
    }
    else if constexpr (std::is_same_v<decltype(property), WarningsAsErrors>)
    {
        warningsAsErrors = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExceptionHandling>)
    {
        exceptionHandling = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AsyncExceptions>)
    {
        asyncExceptions = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RTTI>)
    {
        rtti = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ExternCNoThrow>)
    {
        externCNoThrow = property;
    }
    else if constexpr (std::is_same_v<decltype(property), StdLib>)
    {
        stdLib = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        compileTargetType = property;
    }
    else if constexpr (std::is_same_v<decltype(property), InstructionSet>)
    {
        instructionSet = property;
    }
    else if constexpr (std::is_same_v<decltype(property), CpuType>)
    {
        cpuType = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TranslateInclude>)
    {
        translateInclude = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TreatModuleAsSource>)
    {
        treatModuleAsSource = property;
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        property;
    }
    else
    {
        assignCommonFeature(property);
    }
    if constexpr (sizeof...(properties))
    {
        return ASSIGN(properties...);
    }
    else
    {
        return *this;
    }
}

template <typename T> bool CppSourceTarget::EVALUATE(T property) const
{
    if constexpr (std::is_same_v<decltype(property), Compiler>)
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
    else if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        return compileTargetType == property;
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
        compiler = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
}

template <Dependency dependency, typename T> void CppSourceTarget::assignCommonFeature(T property)
{
    if constexpr (std::is_same_v<decltype(property), TargetOS>)
    {
        targetOs = property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugSymbols>)
    {
        debugSymbols = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Profiling>)
    {
        profiling = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Visibility>)
    {
        localVisibility = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressSanitizer>)
    {
        addressSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LeakSanitizer>)
    {
        leakSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), ThreadSanitizer>)
    {
        threadSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), UndefinedSanitizer>)
    {
        undefinedSanitizer = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Coverage>)
    {
        coverage = property;
    }
    else if constexpr (std::is_same_v<decltype(property), LTO>)
    {
        lto = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeLink>)
    {
        runtimeLink = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Arch>)
    {
        arch = property;
    }
    else if constexpr (std::is_same_v<decltype(property), AddressModel>)
    {
        addModel = property;
    }
    else if constexpr (std::is_same_v<decltype(property), DebugStore>)
    {
        debugStore = property;
    }
    else if constexpr (std::is_same_v<decltype(property), RuntimeDebugging>)
    {
        runtimeDebugging = property;
    }
    else
    {
        compiler = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
}

#endif // HMAKE_CPPSOURCETARGET_HPP
