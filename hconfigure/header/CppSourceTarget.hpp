#ifndef HMAKE_CPPSOURCETARGET_HPP
#define HMAKE_CPPSOURCETARGET_HPP
#ifdef USE_HEADER_UNITS
import "BuildTools.hpp";
import "CSourceTarget.hpp";
import "Features.hpp";
import "FeaturesConvenienceFunctions.hpp";
import "HashedCommand.hpp";
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
#include "HashedCommand.hpp"
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

struct InclNodePointerComparator
{
    bool operator()(const InclNode &lhs, const InclNode &rhs) const;
};

struct InclNodeRecord
{
    Node *node = nullptr;
    atomic<unsigned short> numOfTargets = 0;
    InclNodeRecord(Node *node_);
};

struct ResolveRequirePathBTarget final : BTarget
{
    CppSourceTarget *target;
    explicit ResolveRequirePathBTarget(CppSourceTarget *target_);
    void updateBTarget(Builder &builder, unsigned short round) override;
    pstring getTarjanNodeName() const override;
};

struct AdjustHeaderUnitsBTarget final : BTarget
{
    CppSourceTarget *target;
    explicit AdjustHeaderUnitsBTarget(CppSourceTarget *target_);
    void updateBTarget(Builder &builder, unsigned short round) override;
    pstring getTarjanNodeName() const override;
};

// TODO
// HMake currently does not has proper C Support. There is workaround by ASSING(CSourceTargetEnum::YES) call which that
// use -TC flag with MSVC
class CppSourceTarget : public CppCompilerFeatures,
                        public FeatureConvenienceFunctions<CppSourceTarget>,
                        public CSourceTarget
{
    friend struct ResolveRequirePathBTarget;

  public:
    mutex headerUnitsMutex;
    mutex requirePathsMutex;
    mutex moduleDepsAccessMutex;

    // Written mutex locked in round 1 updateBTarget
    set<SMFile, CompareSourceNode> headerUnits;

    map<InclNode, CppSourceTarget *, InclNodePointerComparator> usageRequirementHuDirs;
    map<InclNode, CppSourceTarget *, InclNodePointerComparator> requirementHuDirs;

    // Written mutex locked in round 1 updateBTarget.
    // Which require is provided by which SMFile
    map<pstring, SMFile *> requirePaths;

    using BaseType = CSourceTarget;
    unique_ptr<PValue> targetBuildCache;
    TargetType compileTargetType;
    /*    ModuleScopeDataOld *moduleScopeData = nullptr;
        inline static map<const CppSourceTarget *, unique_ptr<ModuleScopeDataOld>> moduleScopes;*/
    friend struct PostCompile;
    // Parsed Info Not Changed Once Read
    // pstring targetFilePath;
    pstring buildCacheFilesDirPath;

    // Compile Command excluding source-file or source-files(in case of module) that is also stored in the cache.
    pstring compileCommand;
    pstring sourceCompileCommandPrintFirstHalf;

    // Compile Command including tool. Tool is separated from compile command because on Windows, resource-file needs to
    // be used.
    HashedCommand compileCommandWithTool;

    set<SourceNode, CompareSourceNode> sourceFileDependencies;
    // Comparator used is same as for SourceNode
    set<SMFile, CompareSourceNode> moduleSourceFileDependencies;

    ResolveRequirePathBTarget resolveRequirePathBTarget{this};
    AdjustHeaderUnitsBTarget adjustHeaderUnitsBTarget{this};

    // Set to true if a source or smrule is updated so that latest cache could be stored.
    std::atomic<bool> targetCacheChanged = false;

    void setCpuType();
    bool isCpuTypeG7();

    void setCompileCommand();
    void setSourceCompileCommandPrintFirstHalf();
    inline pstring &getSourceCompileCommandPrintFirstHalf();

    void readBuildCacheFile(Builder &);
    void resolveRequirePaths();
    void populateSourceNodes();
    void parseModuleSourceFiles(Builder &builder);
    void populateResolveRequirePathDependencies();
    pstring getInfrastructureFlags(bool showIncludes) const;
    pstring getCompileCommandPrintSecondPart(const SourceNode &sourceNode) const;
    pstring getCompileCommandPrintSecondPartSMRule(const SMFile &smFile) const;
    PostCompile CompileSMFile(const SMFile &smFile);
    pstring getExtension() const;
    PostCompile updateSourceNodeBTarget(SourceNode &sourceNode);

    PostCompile GenerateSMRulesFile(const SMFile &smFile, bool printOnlyOnError);
    void saveBuildCache(bool round);

    set<SourceDirectory> regexSourceDirs;
    set<SourceDirectory> regexModuleDirs;

    // requirementIncludes size before populateTransitiveProperties function is called
    unsigned short reqIncSizeBeforePopulate = 0;

    RAPIDJSON_DEFAULT_ALLOCATOR cppAllocator;
    size_t buildCacheIndex = UINT64_MAX;
    atomic<size_t> newHeaderUnitsSize = 0;
    bool archiving = false;
    bool archived = false;

    // This function is called in SMFile::decrementTotalSMRuleFileCount once all module files and header-units of our
    // target and dependent targets have been scanned
    static void updateRound1();
    void updateBTarget(Builder &builder, unsigned short round) override;
    /*    void setJson() override; */
    pstring getTarjanNodeName() const override;
    CompilerFlags getCompilerFlags();

    CppSourceTarget(pstring name_, TargetType targetType);
    CppSourceTarget(bool buildExplicit, pstring name_, TargetType targetType);

    void getObjectFiles(vector<const ObjectFile *> *objectFiles,
                        LinkOrArchiveTarget *linkOrArchiveTarget) const override;
    void populateTransitiveProperties();
    void adjustHeaderUnitsPValueArrayPointers();
    CSourceTargetType getCSourceTargetType() const override;

    CppSourceTarget &assignStandardIncludesToPublicHUDirectories();
    // TODO
    // Also provide function overload for functions like publicIncludes here and in CPT
    template <typename... U> CppSourceTarget &publicIncludes(const pstring &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &privateIncludes(const pstring &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &interfaceIncludes(const pstring &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &publicHUIncludes(const pstring &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &privateHUIncludes(const pstring &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &interfaceHUIncludes(const pstring &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &publicHUDirectories(const pstring &include, U... includeDirectoryPString);
    template <typename... U>
    CppSourceTarget &privateHUDirectories(const pstring &include, U... includeDirectoryPString);
    template <typename... U>
    CppSourceTarget &interfaceHUDirectories(const pstring &include, U... includeDirectoryPString);
    CppSourceTarget &publicCompilerFlags(const pstring &compilerFlags);
    CppSourceTarget &privateCompilerFlags(const pstring &compilerFlags);
    CppSourceTarget &interfaceCompilerFlags(const pstring &compilerFlags);
    CppSourceTarget &publicCompileDefinition(const pstring &cddName, const pstring &cddValue = "");
    CppSourceTarget &privateCompileDefinition(const pstring &cddName, const pstring &cddValue = "");
    CppSourceTarget &interfaceCompileDefinition(const pstring &cddName, const pstring &cddValue = "");
    template <typename... U> CppSourceTarget &sourceFiles(const pstring &srcFile, U... sourceFilePString);
    template <typename... U> CppSourceTarget &moduleFiles(const pstring &modFile, U... moduleFilePString);
    template <typename... U> CppSourceTarget &interfaceFiles(const pstring &modFile, U... moduleFilePString);
    void parseRegexSourceDirs(bool assignToSourceNodes, bool recursive, const SourceDirectory &dir);
    template <typename... U> CppSourceTarget &sourceDirectories(const pstring &sourceDirectory, U... directories);
    template <typename... U> CppSourceTarget &moduleDirectories(const pstring &moduleDirectory, U... directories);
    template <typename... U>
    CppSourceTarget &sourceDirectoriesRE(const pstring &sourceDirectory, const pstring &regex, U... directories);
    template <typename... U>
    CppSourceTarget &moduleDirectoriesRE(const pstring &moduleDirectory, const pstring &regex, U... directories);
    template <typename... U> CppSourceTarget &rSourceDirectories(const pstring &sourceDirectory, U... directories);
    template <typename... U> CppSourceTarget &rModuleDirectories(const pstring &moduleDirectory, U... directories);
    template <typename... U>
    CppSourceTarget &rSourceDirectoriesRE(const pstring &sourceDirectory, const pstring &regex, U... directories);
    template <typename... U>
    CppSourceTarget &rModuleDirectoriesRE(const pstring &moduleDirectory, const pstring &regex, U... directories);
    //
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Property>
    CppSourceTarget &assign(T property, Property... properties);
    template <typename T> bool evaluate(T property) const;
    template <Dependency dependency = Dependency::PRIVATE, typename T> void assignCommonFeature(T property);
}; // class Target
bool operator<(const CppSourceTarget &lhs, const CppSourceTarget &rhs);

template <typename... U>
CppSourceTarget &CppSourceTarget::publicIncludes(const pstring &include, U... includeDirectoryPString)
{
    Node *node = Node::getNodeFromNonNormalizedPath(include, false);
    InclNode::emplaceInList(requirementIncludes, node);
    InclNode::emplaceInList(usageRequirementIncludes, node);
    if constexpr (sizeof...(includeDirectoryPString))
    {
        return publicIncludes(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateIncludes(const pstring &include, U... includeDirectoryPString)
{
    InclNode::emplaceInList(requirementIncludes, Node::getNodeFromNonNormalizedPath(include, false));
    if constexpr (sizeof...(includeDirectoryPString))
    {
        return privateIncludes(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceIncludes(const pstring &include, U... includeDirectoryPString)
{
    InclNode::emplaceInList(usageRequirementIncludes, Node::getNodeFromNonNormalizedPath(include, false));
    if constexpr (sizeof...(includeDirectoryPString))
    {
        return interfaceIncludes(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicHUIncludes(const pstring &include, U... includeDirectoryPString)
{
    Node *node = Node::getNodeFromNonNormalizedPath(include, false);
    if (bool added = InclNode::emplaceInList(usageRequirementIncludes, node); added)
    {
        usageRequirementHuDirs.emplace(usageRequirementIncludes.back(), this);
    }
    if (bool added = InclNode::emplaceInList(requirementIncludes, node); added)
    {
        requirementHuDirs.emplace(requirementIncludes.back(), this);
    }

    if constexpr (sizeof...(includeDirectoryPString))
    {
        return publicHUIncludes(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateHUIncludes(const pstring &include, U... includeDirectoryPString)
{
    if (bool added = InclNode::emplaceInList(requirementIncludes, Node::getNodeFromNonNormalizedPath(include, false));
        added)
    {
        requirementHuDirs.emplace(requirementIncludes.back(), this);
    }
    if constexpr (sizeof...(includeDirectoryPString))
    {
        return privateHUIncludes(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceHUIncludes(const pstring &include, U... includeDirectoryPString)
{
    Node *node = Node::getNodeFromNonNormalizedPath(include, false);
    if (bool added = InclNode::emplaceInList(usageRequirementIncludes, node); added)
    {
        usageRequirementHuDirs.emplace(usageRequirementIncludes.back(), this);
    }

    if constexpr (sizeof...(includeDirectoryPString))
    {
        return interfaceHUIncludes(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicHUDirectories(const pstring &include, U... includeDirectoryPString)
{
    requirementHuDirs.emplace(Node::getNodeFromNonNormalizedPath(include, false), this);
    usageRequirementHuDirs.emplace(Node::getNodeFromNonNormalizedPath(include, false), this);

    if constexpr (sizeof...(includeDirectoryPString))
    {
        return publicHUDirectories(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateHUDirectories(const pstring &include, U... includeDirectoryPString)
{
    requirementHuDirs.emplace(Node::getNodeFromNonNormalizedPath(include, false), this);

    if constexpr (sizeof...(includeDirectoryPString))
    {
        return privateHUDirectories(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceHUDirectories(const pstring &include, U... includeDirectoryPString)
{
    usageRequirementHuDirs.emplace(Node::getNodeFromNonNormalizedPath(include, false), this);

    if constexpr (sizeof...(includeDirectoryPString))
    {
        return interfaceHUDirectories(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppSourceTarget &CppSourceTarget::sourceFiles(const pstring &srcFile, U... sourceFilePString)
{
    Node *node = Node::getNodeFromNonNormalizedPath(srcFile, true);
    if (!sourceFileDependencies.contains(node))
    {
        sourceFileDependencies.emplace(this, node);
    }
    if constexpr (sizeof...(sourceFilePString))
    {
        return sourceFiles(sourceFilePString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppSourceTarget &CppSourceTarget::moduleFiles(const pstring &modFile, U... moduleFilePString)
{
    if (evaluate(TreatModuleAsSource::YES))
    {
        return sourceFiles(modFile, moduleFilePString...);
    }

    if (Node *node = Node::getNodeFromNonNormalizedPath(modFile, true); !moduleSourceFileDependencies.contains(node))
    {
        moduleSourceFileDependencies.emplace(this, node);
    }
    if constexpr (sizeof...(moduleFilePString))
    {
        return moduleFiles(moduleFilePString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceFiles(const pstring &modFile, U... moduleFilePString)
{
    if (evaluate(TreatModuleAsSource::YES))
    {
        return sourceFiles(modFile, moduleFilePString...);
    }
    else
    {
        const SMFile &smFile = moduleSourceFileDependencies
                                   .emplace(this, const_cast<Node *>(Node::getNodeFromNonNormalizedPath(modFile, true)))
                                   .first.
                                   operator*();
        const_cast<SMFile &>(smFile).isInterface = true;
        if constexpr (sizeof...(moduleFilePString))
        {
            return interfaceFiles(moduleFilePString...);
        }
        else
        {
            return *this;
        }
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::sourceDirectories(const pstring &sourceDirectory, U... directories)
{
    if (const auto &[pos, Ok] = regexSourceDirs.emplace(sourceDirectory, ".*"); Ok)
    {
        parseRegexSourceDirs(true, false, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return sourceDirectories(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::moduleDirectories(const pstring &moduleDirectory, U... directories)
{
    if (const auto &[pos, Ok] = regexModuleDirs.emplace(moduleDirectory, ".*"); Ok)
    {
        parseRegexSourceDirs(false, false, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return moduleDirectories(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::sourceDirectoriesRE(const pstring &sourceDirectory, const pstring &regex,
                                                      U... directories)
{
    if (const auto &[pos, Ok] = regexSourceDirs.emplace(sourceDirectory, regex); Ok)
    {
        parseRegexSourceDirs(true, false, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return sourceDirectoriesRE(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::moduleDirectoriesRE(const pstring &moduleDirectory, const pstring &regex,
                                                      U... directories)
{
    if (const auto &[pos, Ok] = regexModuleDirs.emplace(moduleDirectory, regex); Ok)
    {
        parseRegexSourceDirs(false, false, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return moduleDirectoriesRE(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::rSourceDirectories(const pstring &sourceDirectory, U... directories)
{
    if (const auto &[pos, Ok] = regexSourceDirs.emplace(sourceDirectory, ".*", true); Ok)
    {
        parseRegexSourceDirs(true, true, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return rSourceDirectories(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::rModuleDirectories(const pstring &moduleDirectory, U... directories)
{
    if (const auto &[pos, Ok] = regexModuleDirs.emplace(moduleDirectory, ".*", true); Ok)
    {
        parseRegexSourceDirs(false, true, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return rModuleDirectories(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::rSourceDirectoriesRE(const pstring &sourceDirectory, const pstring &regex,
                                                       U... directories)
{
    if (const auto &[pos, Ok] = regexSourceDirs.emplace(sourceDirectory, regex, true); Ok)
    {
        parseRegexSourceDirs(true, true, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return R_sourceDirectoriesRE(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::rModuleDirectoriesRE(const pstring &moduleDirectory, const pstring &regex,
                                                       U... directories)
{
    if (const auto &[pos, Ok] = regexModuleDirs.emplace(moduleDirectory, regex, true); Ok)
    {
        parseRegexSourceDirs(false, true, *pos);
    }
    if constexpr (sizeof...(directories))
    {
        return R_moduleDirectoriesRE(directories...);
    }
    return *this;
}

template <Dependency dependency, typename T, typename... Property>
CppSourceTarget &CppSourceTarget::assign(T property, Property... properties)
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
    else if constexpr (std::is_same_v<decltype(property), StaticSourceDirs>)
    {
        staticSourceDirs = property;
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
        return assign(properties...);
    }
    else
    {
        return *this;
    }
}

template <typename T> bool CppSourceTarget::evaluate(T property) const
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
    else if constexpr (std::is_same_v<decltype(property), StaticSourceDirs>)
    {
        return staticSourceDirs == property;
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
