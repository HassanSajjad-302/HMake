#ifndef HMAKE_CPPSOURCETARGET_HPP
#define HMAKE_CPPSOURCETARGET_HPP
#ifdef USE_HEADER_UNITS
import "BuildTools.hpp";
import "CSourceTarget.hpp";
import "Features.hpp";
import "FeaturesConvenienceFunctions.hpp";
import "JConsts.hpp";
import "PostBasic.hpp";
import "ToolsCache.hpp";
import <concepts>;
import <set>;
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
#endif

using std::set, std::same_as;

struct SourceDirectory
{
    const Node *sourceDirectory;
    pstring regex;
    bool recursive;
    SourceDirectory() = default;
    SourceDirectory(const pstring &sourceDirectory_, pstring regex_, bool recursive_ = false);
};
void to_json(Json &j, const SourceDirectory &sourceDirectory);
bool operator<(const SourceDirectory &lhs, const SourceDirectory &rhs);

struct CompilerFlags
{
    // GCC
    pstring OPTIONS;
    pstring OPTIONS_COMPILE_CPP;
    pstring OPTIONS_COMPILE;
    pstring DEFINES_COMPILE_CPP;
    pstring LANG;

    pstring TRANSLATE_INCLUDE;
    // MSVC
    pstring DOT_CC_COMPILE;
    pstring DOT_ASM_COMPILE;
    pstring DOT_ASM_OUTPUT_COMPILE;
    pstring DOT_LD_ARCHIVE;
    pstring PCH_FILE_COMPILE;
    pstring PCH_SOURCE_COMPILE;
    pstring PCH_HEADER_COMPILE;
    pstring PDB_CFLAG;
    pstring ASMFLAGS_ASM;
    pstring CPP_FLAGS_COMPILE_CPP;
    pstring CPP_FLAGS_COMPILE;
};

struct ModuleScopeData
{
    mutex smFilesMutex;
    mutex headerUnitsMutex;
    mutex requirePathsMutex;

    // Written mutex locked in round 1 preSort.
    // TODO This should be with a comparator comparing file paths instead of just pointer comparisons
    set<SMFile *> smFiles;

    // Written mutex locked in round 1 updateBTarget
    set<SMFile, CompareSourceNode> headerUnits;

    // Which header unit directory come from which target
    map<const InclNode *, CppSourceTarget *> huDirTarget;

    // Written mutex locked in round 1 updateBTarget.
    // Which require is provided by which SMFile
    map<pstring, SMFile *> requirePaths;

    set<CppSourceTarget *> targets;
    atomic<unsigned int> totalSMRuleFileCount = 0;
};

// TODO
// HMake currently does not has proper C Support. There is workaround by ASSING(CSourceTargetEnum::YES) call which that
// use -TC flag with MSVC
class CppSourceTarget : public CppCompilerFeatures,
                        public CTarget,
                        public FeatureConvenienceFunctions<CppSourceTarget>,
                        public CSourceTarget
{
  public:
    using BaseType = CSourceTarget;
    // Written mutex locked in round 1 updateBTarget
    mutex headerUnitsMutex;
    unique_ptr<PValue> targetBuildCache;
    TargetType compileTargetType;
    CppSourceTarget *moduleScope = nullptr;
    ModuleScopeData *moduleScopeData = nullptr;
    inline static map<const CppSourceTarget *, unique_ptr<ModuleScopeData>> moduleScopes;
    friend struct PostCompile;
    // Parsed Info Not Changed Once Read
    pstring targetFilePath;
    pstring buildCacheFilesDirPath;

    // Compile Command excluding source-file or source-files(in case of module) that is also stored in the cache.
    pstring compileCommand;
    // Compile Command including tool. Tool is separated from compile command because on Windows, resource-file needs to
    // be used.
    pstring compileCommandWithTool;
    pstring sourceCompileCommandPrintFirstHalf;

    set<SourceNode, CompareSourceNode> sourceFileDependencies;
    // Comparator used is same as for SourceNode
    set<SMFile, CompareSourceNode> moduleSourceFileDependencies;
    // Set to true if a source or smrule is updated so that latest cache could be stored.
    std::atomic<bool> targetCacheChanged = false;
    void setCpuType();
    bool isCpuTypeG7();

    set<const SMFile *> headerUnits;

    void setCompileCommand();
    void setSourceCompileCommandPrintFirstHalf();
    inline pstring &getSourceCompileCommandPrintFirstHalf();

    void readBuildCacheFile(Builder &);
    void resolveRequirePaths();
    void populateSourceNodes();
    void parseModuleSourceFiles(Builder &builder);
    pstring getInfrastructureFlags(bool showIncludes);
    pstring getCompileCommandPrintSecondPart(const SourceNode &sourceNode);
    pstring getCompileCommandPrintSecondPartSMRule(const SMFile &smFile);
    PostCompile CompileSMFile(SMFile &smFile);
    pstring getExtension();
    PostCompile updateSourceNodeBTarget(SourceNode &sourceNode);

    PostCompile GenerateSMRulesFile(const SMFile &smFile, bool printOnlyOnError);
    void saveBuildCache(bool round);

    set<SourceDirectory> regexSourceDirs;
    set<SourceDirectory> regexModuleDirs;
    // requirementIncludes size before populateTransitiveProperties function is called
    unsigned short reqIncSizeBeforePopulate = 0;

    RAPIDJSON_DEFAULT_ALLOCATOR cppAllocator;
    size_t buildCacheIndex = UINT64_MAX;
    size_t newHeaderUnitsSize = 0;
    bool archiving = false;
    bool archived = false;

    void preSort(Builder &builder, unsigned short round) override;
    void updateBTarget(Builder &builder, unsigned short round) override;
    /*    void setJson() override; */
    pstring getTarjanNodeName() const override;
    BTarget *getBTarget() override;
    C_Target *get_CAPITarget(BSMode bsMode) override;
    CompilerFlags getCompilerFlags();

  public:
    CppSourceTarget(pstring name_, TargetType targetType);
    CppSourceTarget(pstring name_, TargetType targetType, CTarget &other, bool hasFile = true);

    void getObjectFiles(vector<const ObjectFile *> *objectFiles,
                        LinkOrArchiveTarget *linkOrArchiveTarget) const override;
    void populateTransitiveProperties();
    void registerHUInclNode(const InclNode &inclNode);

    CppSourceTarget &setModuleScope(CppSourceTarget *moduleScope_);
    CppSourceTarget &setModuleScope();

    CppSourceTarget &assignStandardIncludesToHUIncludes();
    // TODO
    // Also provide function overload for functions like PUBLIC_INCLUDES here and in CPT
    template <typename... U> CppSourceTarget &PUBLIC_INCLUDES(const pstring &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &PRIVATE_INCLUDES(const pstring &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &INTERFACE_INCLUDES(const pstring &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &PUBLIC_HU_INCLUDES(const pstring &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &PRIVATE_HU_INCLUDES(const pstring &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &HU_DIRECTORIES(const pstring &include, U... includeDirectoryPString);
    CppSourceTarget &PUBLIC_COMPILER_FLAGS(const pstring &compilerFlags);
    CppSourceTarget &PRIVATE_COMPILER_FLAGS(const pstring &compilerFlags);
    CppSourceTarget &INTERFACE_COMPILER_FLAGS(const pstring &compilerFlags);
    CppSourceTarget &PUBLIC_COMPILE_DEFINITION(const pstring &cddName, const pstring &cddValue = "");
    CppSourceTarget &PRIVATE_COMPILE_DEFINITION(const pstring &cddName, const pstring &cddValue = "");
    CppSourceTarget &INTERFACE_COMPILE_DEFINITION(const pstring &cddName, const pstring &cddValue = "");
    template <typename... U> CppSourceTarget &SOURCE_FILES(const pstring &srcFile, U... sourceFilePString);
    template <typename... U> CppSourceTarget &MODULE_FILES(const pstring &modFile, U... moduleFilePString);
    void parseRegexSourceDirs(bool assignToSourceNodes, bool recursive, const SourceDirectory &dir);
    template <typename... U> CppSourceTarget &SOURCE_DIRECTORIES(const pstring &sourceDirectory, U... directories);
    template <typename... U> CppSourceTarget &MODULE_DIRECTORIES(const pstring &moduleDirectory, U... directories);
    template <typename... U>
    CppSourceTarget &SOURCE_DIRECTORIES_RG(const pstring &sourceDirectory, const pstring &regex, U... directories);
    template <typename... U>
    CppSourceTarget &MODULE_DIRECTORIES_RG(const pstring &moduleDirectory, const pstring &regex, U... directories);
    template <typename... U> CppSourceTarget &R_SOURCE_DIRECTORIES(const pstring &sourceDirectory, U... directories);
    template <typename... U> CppSourceTarget &R_MODULE_DIRECTORIES(const pstring &moduleDirectory, U... directories);
    template <typename... U>
    CppSourceTarget &R_SOURCE_DIRECTORIES_RG(const pstring &sourceDirectory, const pstring &regex, U... directories);
    template <typename... U>
    CppSourceTarget &R_MODULE_DIRECTORIES_RG(const pstring &moduleDirectory, const pstring &regex, U... directories);
    //
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Property>
    CppSourceTarget &ASSIGN(T property, Property... properties);
    template <typename T> bool EVALUATE(T property) const;
    template <Dependency dependency = Dependency::PRIVATE, typename T> void assignCommonFeature(T property);
}; // class Target
bool operator<(const CppSourceTarget &lhs, const CppSourceTarget &rhs);

template <typename... U>
CppSourceTarget &CppSourceTarget::PUBLIC_INCLUDES(const pstring &include, U... includeDirectoryPString)
{
    Node *node = Node::getNodeFromNonNormalizedPath(include, false);
    InclNode::emplaceInList(requirementIncludes, node);
    InclNode::emplaceInList(usageRequirementIncludes, node);
    if constexpr (sizeof...(includeDirectoryPString))
    {
        return PUBLIC_INCLUDES(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::PRIVATE_INCLUDES(const pstring &include, U... includeDirectoryPString)
{
    InclNode::emplaceInList(requirementIncludes, Node::getNodeFromNonNormalizedPath(include, false));
    if constexpr (sizeof...(includeDirectoryPString))
    {
        return PRIVATE_INCLUDES(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::INTERFACE_INCLUDES(const pstring &include, U... includeDirectoryPString)
{
    InclNode::emplaceInList(usageRequirementIncludes, Node::getNodeFromNonNormalizedPath(include, false));
    if constexpr (sizeof...(includeDirectoryPString))
    {
        return INTERFACE_INCLUDES(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::PUBLIC_HU_INCLUDES(const pstring &include, U... includeDirectoryPString)
{
    Node *node = Node::getNodeFromNonNormalizedPath(include, false);
    InclNode::emplaceInList(usageRequirementIncludes, node);
    if (bool added = InclNode::emplaceInList(requirementIncludes, node); added)
    {
        registerHUInclNode(requirementIncludes.back());
    }

    if constexpr (sizeof...(includeDirectoryPString))
    {
        return PUBLIC_HU_INCLUDES(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::PRIVATE_HU_INCLUDES(const pstring &include, U... includeDirectoryPString)
{
    if (bool added = InclNode::emplaceInList(requirementIncludes, Node::getNodeFromNonNormalizedPath(include, false));
        added)
    {
        registerHUInclNode(requirementIncludes.back());
    }
    if constexpr (sizeof...(includeDirectoryPString))
    {
        return PRIVATE_HU_INCLUDES(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::HU_DIRECTORIES(const pstring &include, U... includeDirectoryPString)
{
    registerHUInclNode(targets<InclNode>.emplace(Node::getNodeFromNonNormalizedPath(include, false)).first.operator*());

    if constexpr (sizeof...(includeDirectoryPString))
    {
        return HU_DIRECTORIES(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppSourceTarget &CppSourceTarget::SOURCE_FILES(const pstring &srcFile, U... sourceFilePString)
{
    sourceFileDependencies.emplace(this, const_cast<Node *>(Node::getNodeFromNonNormalizedPath(srcFile, true)));
    if constexpr (sizeof...(sourceFilePString))
    {
        return SOURCE_FILES(sourceFilePString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppSourceTarget &CppSourceTarget::MODULE_FILES(const pstring &modFile, U... moduleFilePString)
{
    if (EVALUATE(TreatModuleAsSource::YES))
    {
        return SOURCE_FILES(modFile, moduleFilePString...);
    }
    else
    {
        moduleSourceFileDependencies.emplace(this,
                                             const_cast<Node *>(Node::getNodeFromNonNormalizedPath(modFile, true)));
        if constexpr (sizeof...(moduleFilePString))
        {
            return MODULE_FILES(moduleFilePString...);
        }
        else
        {
            return *this;
        }
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::SOURCE_DIRECTORIES(const pstring &sourceDirectory, U... directories)
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
CppSourceTarget &CppSourceTarget::MODULE_DIRECTORIES(const pstring &moduleDirectory, U... directories)
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
CppSourceTarget &CppSourceTarget::SOURCE_DIRECTORIES_RG(const pstring &sourceDirectory, const pstring &regex,
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
CppSourceTarget &CppSourceTarget::MODULE_DIRECTORIES_RG(const pstring &moduleDirectory, const pstring &regex,
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
CppSourceTarget &CppSourceTarget::R_SOURCE_DIRECTORIES(const pstring &sourceDirectory, U... directories)
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
CppSourceTarget &CppSourceTarget::R_MODULE_DIRECTORIES(const pstring &moduleDirectory, U... directories)
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
CppSourceTarget &CppSourceTarget::R_SOURCE_DIRECTORIES_RG(const pstring &sourceDirectory, const pstring &regex,
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
CppSourceTarget &CppSourceTarget::R_MODULE_DIRECTORIES_RG(const pstring &moduleDirectory, const pstring &regex,
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
    if constexpr (std::is_same_v<decltype(property), CSourceTargetEnum>)
    {
        cSourceTarget = property;
    }
    else if constexpr (std::is_same_v<decltype(property), Compiler>)
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
    if constexpr (std::is_same_v<decltype(property), CSourceTargetEnum>)
    {
        return cSourceTarget == property;
    }
    else if constexpr (std::is_same_v<decltype(property), Compiler>)
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
