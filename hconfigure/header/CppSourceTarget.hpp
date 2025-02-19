#ifndef HMAKE_CPPSOURCETARGET_HPP
#define HMAKE_CPPSOURCETARGET_HPP
#ifdef USE_HEADER_UNITS
import "BuildTools.hpp";
import "CSourceTarget.hpp";
import "FeaturesConvenienceFunctions.hpp";
import "HashedCommand.hpp";
import "JConsts.hpp";
import "RunCommand.hpp";
import "SMFile.hpp";
import "ToolsCache.hpp";
import <concepts>;
import <set>;
#else
#include "BuildTools.hpp"
#include "CSourceTarget.hpp"
#include "FeaturesConvenienceFunctions.hpp"
#include "HashedCommand.hpp"
#include "JConsts.hpp"
#include "RunCommand.hpp"
#include "SMFile.hpp"
#include "ToolsCache.hpp"
#include <concepts>
#endif

using std::same_as;

struct SourceDirectory
{
    const Node *sourceDirectory;
    string regex;
    bool recursive;
    SourceDirectory(const string &sourceDirectory_, string regex_, bool recursive_ = false);
};

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

struct InclNodePointerComparator
{
    bool operator()(const InclNode &lhs, const InclNode &rhs) const;
};

struct ResolveRequirePathBTarget final : BTarget
{
    CppSourceTarget *target;
    explicit ResolveRequirePathBTarget(CppSourceTarget *target_);
    void updateBTarget(Builder &builder, unsigned short round) override;
    string getTarjanNodeName() const override;
};

// TODO
// Remove this. Instead use CppSourceTarget itself.
struct AdjustHeaderUnitsBTarget final : BTarget
{
    CppSourceTarget *target;
    explicit AdjustHeaderUnitsBTarget(CppSourceTarget *target_);
    void updateBTarget(Builder &builder, unsigned short round) override;
    string getTarjanNodeName() const override;
};

struct RequireNameTargetId
{
    uint64_t id;
    string requireName;
    RequireNameTargetId(uint64_t id_, string requirePath_);
    bool operator==(const RequireNameTargetId &other) const;
};

struct RequireNameTargetIdHash
{
    uint64_t operator()(const RequireNameTargetId &req) const;
};
inline phmap::parallel_flat_hash_map_m<RequireNameTargetId, SMFile *, RequireNameTargetIdHash> requirePaths2;

// TODO
// HMake currently does not has proper C Support. There is workaround by ASSING(CSourceTargetEnum::YES) call which that
// use -TC flag with MSVC
class CppSourceTarget : public CppCompilerFeatures,
                        public FeatureConvenienceFunctions<CppSourceTarget>,
                        public CSourceTarget
{
    struct SMFileEqual
    {
        using is_transparent = void;

        bool operator()(const SMFile *lhs, const SMFile *rhs) const;
        bool operator()(const SMFile *lhs, const Node *rhs) const;
        bool operator()(const Node *lhs, const SMFile *rhs) const;
    };

    struct SMFileHash
    {
        using is_transparent = void; // or std::equal_to<>

        std::size_t operator()(const SMFile *node) const;
        std::size_t operator()(const Node *node) const;
    };

    friend struct ResolveRequirePathBTarget;

  public:
    mutex headerUnitsMutex;
    mutex moduleDepsAccessMutex;

    // Written mutex locked in round 1 updateBTarget
    phmap::flat_hash_set<SMFile *, SMFileHash, SMFileEqual> headerUnits;

    vector<InclNodeTargetMap> useReqHuDirs;
    vector<InclNodeTargetMap> reqHuDirs;

    using BaseType = CSourceTarget;

    TargetType compileTargetType;
    friend struct PostCompile;

    // Compile Command excluding source-file or source-files(in case of module) that is also stored in the cache.
    string compileCommand;
    string sourceCompileCommandPrintFirstHalf;

    // Compile Command including tool. Tool is separated from compile command because on Windows, resource-file needs to
    // be used.
    HashedCommand compileCommandWithTool;

    vector<SourceNode> srcFileDeps;
    // Comparator used is same as for SourceNode
    vector<SMFile> modFileDeps;

    vector<SMFile> oldHeaderUnits;

    ResolveRequirePathBTarget resolveRequirePathBTarget{this};
    AdjustHeaderUnitsBTarget adjustHeaderUnitsBTarget{this};

    Node *buildCacheFilesDirPathNode = nullptr;
    // requirementIncludes size before populateTransitiveProperties function is called
    unsigned short reqIncSizeBeforePopulate = 0;

    atomic<uint64_t> newHeaderUnitsSize = 0;
    bool archiving = false;
    bool archived = false;

    // Set to true if a source or smrule of a module is updated so that latest cache could be stored.
    bool moduleFileScanned = false;
    // set to true if a source or smrule of a header-unit is updated so that latest cache could be stored.
    bool headerUnitScanned = false;

    void setCpuType();
    bool isCpuTypeG7();

    void setCompileCommand();
    void setSourceCompileCommandPrintFirstHalf();
    inline string &getSourceCompileCommandPrintFirstHalf();

    string getDependenciesPString() const;
    void resolveRequirePaths();
    void populateSourceNodes();
    void parseModuleSourceFiles(Builder &builder);
    void populateSourceNodesConfigureTime();
    void parseModuleSourceFilesConfigureTime(Builder &builder);
    void populateResolveRequirePathDependencies();
    string getInfrastructureFlags(bool showIncludes) const;
    string getCompileCommandPrintSecondPart(const SourceNode &sourceNode) const;
    string getCompileCommandPrintSecondPartSMRule(const SMFile &smFile) const;
    PostCompile CompileSMFile(const SMFile &smFile);
    string getExtension() const;
    PostCompile updateSourceNodeBTarget(const SourceNode &sourceNode);

    PostCompile GenerateSMRulesFile(const SMFile &smFile, bool printOnlyOnError);
    void saveBuildCache(bool round);
    void updateBTarget(Builder &builder, unsigned short round) override;
    void copyJson() override;
    void writeTargetConfigCacheAtConfigureTime(bool before);
    void readConfigCacheAtBuildTime();
    string getTarjanNodeName() const override;
    CompilerFlags getCompilerFlags();

    CppSourceTarget(const string &name_, TargetType targetType);
    CppSourceTarget(bool buildExplicit, const string &name_, TargetType targetType);
    CppSourceTarget(const string &name_, TargetType targetType, Configuration *configuration_);
    CppSourceTarget(bool buildExplicit, const string &name_, TargetType targetType, Configuration *configuration_);

    CppSourceTarget(string buildCacheFilesDirPath_, const string &name_, TargetType targetType);
    CppSourceTarget(string buildCacheFilesDirPath_, bool buildExplicit, const string &name_, TargetType targetType);
    CppSourceTarget(string buildCacheFilesDirPath_, const string &name_, TargetType targetType,
                    Configuration *configuration_);
    CppSourceTarget(string buildCacheFilesDirPath_, bool buildExplicit, const string &name_, TargetType targetType,
                    Configuration *configuration_);

    void initializeCppSourceTarget(TargetType targetType, const string &name_, string buildCacheFilesDirPath);

    void getObjectFiles(vector<const ObjectFile *> *objectFiles,
                        LinkOrArchiveTarget *linkOrArchiveTarget) const override;
    void populateTransitiveProperties();
    void adjustHeaderUnitsValueArrayPointers();
    CSourceTargetType getCSourceTargetType() const override;

    CppSourceTarget &makeReqInclsUseable();
    static bool actuallyAddSourceFile(vector<SourceNode> &sourceFiles, const string &sourceFile,
                                      CppSourceTarget *target);
    static bool actuallyAddSourceFile(vector<SourceNode> &sourceFiles, Node *sourceFileNode, CppSourceTarget *target);
    static bool actuallyAddModuleFile(vector<SMFile> &smFiles, const string &moduleFile, CppSourceTarget *target);
    static bool actuallyAddModuleFile(vector<SMFile> &smFiles, Node *moduleFileNode, CppSourceTarget *target);
    void actuallyAddSourceFileConfigTime(const Node *node);
    void actuallyAddModuleFileConfigTime(const Node *node, bool isInterface);
    CppSourceTarget &removeSourceFile(const string &sourceFile);
    CppSourceTarget &removeModuleFile(const string &moduleFile);

    template <typename... U> CppSourceTarget &publicDeps(CppSourceTarget *dep, const U... deps);
    template <typename... U> CppSourceTarget &privateDeps(CppSourceTarget *dep, const U... deps);
    template <typename... U> CppSourceTarget &interfaceDeps(CppSourceTarget *dep, const U... deps);

    template <typename... U> CppSourceTarget &deps(CppSourceTarget *dep, Dependency dependency, const U... deps);

    // TODO
    // Also provide function overload for functions like publicIncludes here and in CPT
    template <typename... U> CppSourceTarget &publicIncludes(const string &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &privateIncludes(const string &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &interfaceIncludes(const string &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &publicHUIncludes(const string &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &privateHUIncludes(const string &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &interfaceHUIncludes(const string &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &publicHUDirectories(const string &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &privateHUDirectories(const string &include, U... includeDirectoryPString);
    template <typename... U>
    CppSourceTarget &interfaceHUDirectories(const string &include, U... includeDirectoryPString);
    CppSourceTarget &publicCompilerFlags(const string &compilerFlags);
    CppSourceTarget &privateCompilerFlags(const string &compilerFlags);
    CppSourceTarget &interfaceCompilerFlags(const string &compilerFlags);
    CppSourceTarget &publicCompileDefinition(const string &cddName, const string &cddValue = "");
    CppSourceTarget &privateCompileDefinition(const string &cddName, const string &cddValue = "");
    CppSourceTarget &interfaceCompileDefinition(const string &cddName, const string &cddValue = "");
    template <typename... U> CppSourceTarget &sourceFiles(const string &srcFile, U... sourceFilePString);
    template <typename... U> CppSourceTarget &moduleFiles(const string &modFile, U... moduleFilePString);
    template <typename... U> CppSourceTarget &interfaceFiles(const string &modFile, U... moduleFilePString);
    void parseRegexSourceDirs(bool assignToSourceNodes, const string &sourceDirectory, string regex, bool recursive);
    template <typename... U> CppSourceTarget &sourceDirectories(const string &sourceDirectory, U... directories);
    template <typename... U> CppSourceTarget &moduleDirectories(const string &moduleDirectory, U... directories);
    template <typename... U>
    CppSourceTarget &sourceDirectoriesRE(const string &sourceDirectory, const string &regex, U... directories);
    template <typename... U>
    CppSourceTarget &moduleDirectoriesRE(const string &moduleDirectory, const string &regex, U... directories);
    template <typename... U> CppSourceTarget &rSourceDirectories(const string &sourceDirectory, U... directories);
    template <typename... U> CppSourceTarget &rModuleDirectories(const string &moduleDirectory, U... directories);
    template <typename... U>
    CppSourceTarget &rSourceDirectoriesRE(const string &sourceDirectory, const string &regex, U... directories);
    template <typename... U>
    CppSourceTarget &rModuleDirectoriesRE(const string &moduleDirectory, const string &regex, U... directories);
    //
    template <Dependency dependency = Dependency::PRIVATE, typename T, typename... Property>
    CppSourceTarget &assign(T property, Property... properties);
    template <typename T> bool evaluate(T property) const;
    template <Dependency dependency = Dependency::PRIVATE, typename T> void assignCommonFeature(T property);
}; // class Target

bool operator<(const CppSourceTarget &lhs, const CppSourceTarget &rhs);

template <typename... U> CppSourceTarget &CppSourceTarget::publicDeps(CppSourceTarget *dep, const U... deps)
{
    requirementDeps.emplace(dep);
    usageRequirementDeps.emplace(dep);
    realBTargets[2].addDependency(*dep);
    if constexpr (sizeof...(deps))
    {
        return publicDeps(deps...);
    }
    return static_cast<CppSourceTarget &>(*this);
}

template <typename... U> CppSourceTarget &CppSourceTarget::privateDeps(CppSourceTarget *dep, const U... deps)
{
    requirementDeps.emplace(dep);
    realBTargets[2].addDependency(*dep);
    if constexpr (sizeof...(deps))
    {
        return privateDeps(deps...);
    }
    return static_cast<CppSourceTarget &>(*this);
}

template <typename... U> CppSourceTarget &CppSourceTarget::interfaceDeps(CppSourceTarget *dep, const U... deps)
{
    usageRequirementDeps.emplace(dep);
    realBTargets[2].addDependency(*dep);
    if constexpr (sizeof...(deps))
    {
        return interfaceDeps(deps...);
    }
    return static_cast<CppSourceTarget &>(*this);
}

template <typename... U>
CppSourceTarget &CppSourceTarget::deps(CppSourceTarget *dep, const Dependency dependency, const U... deps)
{
    if (dependency == Dependency::PUBLIC)
    {
        requirementDeps.emplace(dep);
        usageRequirementDeps.emplace(dep);
        realBTargets[2].addDependency(*dep);
    }
    else if (dependency == Dependency::PRIVATE)
    {
        requirementDeps.emplace(dep);
        realBTargets[2].addDependency(*dep);
    }
    else
    {
        usageRequirementDeps.emplace(dep);
        realBTargets[2].addDependency(*dep);
    }
    if constexpr (sizeof...(deps))
    {
        return DEPS(deps...);
    }
    return static_cast<CppSourceTarget &>(*this);
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicIncludes(const string &include, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        if (useMiniTarget == UseMiniTarget::YES)
        {
        }
        // Initialized in CppSourceTarget round 2
    }
    else
    {
        actuallyAddInclude(reqIncls, include);
        actuallyAddInclude(useReqIncls, include);
    }

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
CppSourceTarget &CppSourceTarget::privateIncludes(const string &include, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        if (useMiniTarget == UseMiniTarget::YES)
        {
        }
        // Initialized in CppSourceTarget round 2
    }
    else
    {
        actuallyAddInclude(reqIncls, include);
    }

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
CppSourceTarget &CppSourceTarget::interfaceIncludes(const string &include, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        if (useMiniTarget == UseMiniTarget::YES)
        {
        }
        // Initialized in CppSourceTarget round 2
    }
    else
    {
        actuallyAddInclude(useReqIncls, include);
    }

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
CppSourceTarget &CppSourceTarget::publicHUIncludes(const string &include, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        if (useMiniTarget == UseMiniTarget::YES)
        {
        }
        // Initialized in CppSourceTarget round 2
    }
    else
    {
        if (evaluate(TreatModuleAsSource::NO))
        {
            actuallyAddInclude(this, reqHuDirs, include);
            actuallyAddInclude(this, useReqHuDirs, include);
            actuallyAddInclude(reqIncls, include);
            actuallyAddInclude(useReqIncls, include);
        }
        else
        {
            actuallyAddInclude(reqIncls, include);
            actuallyAddInclude(useReqIncls, include);
        }
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
CppSourceTarget &CppSourceTarget::privateHUIncludes(const string &include, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        if (useMiniTarget == UseMiniTarget::YES)
        {
        }
        // Initialized in CppSourceTarget round 2
    }
    else
    {
        if (evaluate(TreatModuleAsSource::NO))
        {
            actuallyAddInclude(this, reqHuDirs, include);
            actuallyAddInclude(reqIncls, include);
        }
        else
        {
            actuallyAddInclude(reqIncls, include);
        }
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
CppSourceTarget &CppSourceTarget::interfaceHUIncludes(const string &include, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        if (useMiniTarget == UseMiniTarget::YES)
        {
        }
        // Initialized in CppSourceTarget round 2
    }
    else
    {
        if (evaluate(TreatModuleAsSource::NO))
        {
            actuallyAddInclude(this, useReqHuDirs, include);
            actuallyAddInclude(useReqIncls, include);
        }
        else
        {
            actuallyAddInclude(useReqIncls, include);
        }
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
CppSourceTarget &CppSourceTarget::publicHUDirectories(const string &include, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        if (useMiniTarget == UseMiniTarget::YES)
        {
        }
        // Initialized in CppSourceTarget round 2
    }
    else
    {
        if (evaluate(TreatModuleAsSource::NO))
        {
            actuallyAddInclude(this, reqHuDirs, include);
            actuallyAddInclude(this, useReqHuDirs, include);
        }
    }

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
CppSourceTarget &CppSourceTarget::privateHUDirectories(const string &include, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        if (useMiniTarget == UseMiniTarget::YES)
        {
        }
        // Initialized in CppSourceTarget round 2
    }
    else
    {
        if (evaluate(TreatModuleAsSource::NO))
        {
            actuallyAddInclude(this, reqHuDirs, include);
        }
    }

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
CppSourceTarget &CppSourceTarget::interfaceHUDirectories(const string &include, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        if (useMiniTarget == UseMiniTarget::YES)
        {
        }
        // Initialized in CppSourceTarget round 2
    }
    else
    {
        if (evaluate(TreatModuleAsSource::NO))
        {
            actuallyAddInclude(this, useReqHuDirs, include);
        }
    }

    if constexpr (sizeof...(includeDirectoryPString))
    {
        return interfaceHUDirectories(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppSourceTarget &CppSourceTarget::sourceFiles(const string &srcFile, U... sourceFilePString)
{
    if (evaluate(UseMiniTarget::YES))
    {
        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            actuallyAddSourceFileConfigTime(Node::getNodeFromNonNormalizedString(srcFile, true));
        }
        // Initialized in CppSourceTarget round 2
    }
    else
    {
        actuallyAddSourceFile(srcFileDeps, srcFile, this);
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

template <typename... U> CppSourceTarget &CppSourceTarget::moduleFiles(const string &modFile, U... moduleFilePString)
{
    if (evaluate(TreatModuleAsSource::YES))
    {
        return sourceFiles(modFile, moduleFilePString...);
    }

    if (evaluate(UseMiniTarget::YES))
    {
        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            actuallyAddModuleFileConfigTime(Node::getNodeFromNonNormalizedString(modFile, true), false);
        }
        // Initialized in CppSourceTarget round 2
    }
    else
    {
        actuallyAddModuleFile(modFileDeps, modFile, this);
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

template <typename... U> CppSourceTarget &CppSourceTarget::interfaceFiles(const string &modFile, U... moduleFilePString)
{
    if (evaluate(TreatModuleAsSource::YES))
    {
        return sourceFiles(modFile, moduleFilePString...);
    }

    if (evaluate(UseMiniTarget::YES))
    {
        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            actuallyAddModuleFileConfigTime(Node::getNodeFromNonNormalizedString(modFile, true), true);
        }
        // Initialized in CppSourceTarget round 2
    }
    else
    {
        actuallyAddModuleFile(modFileDeps, modFile, this);
        modFileDeps.end()->isInterface = true;
    }

    if constexpr (sizeof...(moduleFilePString))
    {
        return interfaceFiles(moduleFilePString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::sourceDirectories(const string &sourceDirectory, U... directories)
{
    parseRegexSourceDirs(true, sourceDirectory, ".*", false);
    if constexpr (sizeof...(directories))
    {
        return sourceDirectories(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::moduleDirectories(const string &moduleDirectory, U... directories)
{
    parseRegexSourceDirs(false, moduleDirectory, ".*", false);
    if constexpr (sizeof...(directories))
    {
        return moduleDirectories(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::sourceDirectoriesRE(const string &sourceDirectory, const string &regex,
                                                      U... directories)
{
    parseRegexSourceDirs(true, sourceDirectory, regex, false);
    if constexpr (sizeof...(directories))
    {
        return sourceDirectoriesRE(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::moduleDirectoriesRE(const string &moduleDirectory, const string &regex,
                                                      U... directories)
{
    parseRegexSourceDirs(false, moduleDirectory, regex, false);
    if constexpr (sizeof...(directories))
    {
        return moduleDirectoriesRE(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::rSourceDirectories(const string &sourceDirectory, U... directories)
{
    parseRegexSourceDirs(true, sourceDirectory, ".*", true);
    if constexpr (sizeof...(directories))
    {
        return rSourceDirectories(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::rModuleDirectories(const string &moduleDirectory, U... directories)
{
    parseRegexSourceDirs(false, moduleDirectory, ".*", true);
    if constexpr (sizeof...(directories))
    {
        return rModuleDirectories(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::rSourceDirectoriesRE(const string &sourceDirectory, const string &regex,
                                                       U... directories)
{
    parseRegexSourceDirs(true, sourceDirectory, regex, true);
    if constexpr (sizeof...(directories))
    {
        return R_sourceDirectoriesRE(directories...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::rModuleDirectoriesRE(const string &moduleDirectory, const string &regex,
                                                       U... directories)
{
    parseRegexSourceDirs(false, moduleDirectory, regex, true);
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
    else if constexpr (std::is_same_v<decltype(property), UseMiniTarget>)
    {
        return useMiniTarget == property;
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
