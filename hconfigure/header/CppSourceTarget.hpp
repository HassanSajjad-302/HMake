#ifndef HMAKE_CPPSOURCETARGET_HPP
#define HMAKE_CPPSOURCETARGET_HPP
#ifdef USE_HEADER_UNITS
import "BuildTools.hpp";
import "Configuration.hpp";
import "CSourceTarget.hpp";
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
#include "Configuration.hpp"
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
class CppSourceTarget : public CppTargetFeatures, public CSourceTarget
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

    // Written mutex locked in round 1 updateBTarget
    flat_hash_set<SMFile *, SMFileHash, SMFileEqual> headerUnitsSet;

    vector<InclNodeTargetMap> useReqHuDirs;
    vector<InclNodeTargetMap> reqHuDirs;

    using BaseType = CSourceTarget;

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

    void setCompileCommand();
    void setSourceCompileCommandPrintFirstHalf();
    inline string &getSourceCompileCommandPrintFirstHalf();

    string getDependenciesPString() const;
    void resolveRequirePaths();
    void populateSourceNodes();
    void parseModuleSourceFiles(Builder &builder);
    void populateResolveRequirePathDependencies();
    string getInfrastructureFlags(const Compiler &compiler, bool showIncludes) const;
    string getCompileCommandPrintSecondPart(const SourceNode &sourceNode) const;
    string getCompileCommandPrintSecondPartSMRule(const SMFile &smFile) const;
    PostCompile CompileSMFile(const SMFile &smFile);
    PostCompile updateSourceNodeBTarget(const SourceNode &sourceNode);

    PostCompile GenerateSMRulesFile(const SMFile &smFile, bool printOnlyOnError);
    void saveBuildCache(bool round);
    void updateBTarget(Builder &builder, unsigned short round) override;
    void copyJson() override;
    void writeTargetConfigCacheAtConfigureTime(bool before);
    void readConfigCacheAtBuildTime();
    string getTarjanNodeName() const override;

    CppSourceTarget(const string &name_);
    CppSourceTarget(bool buildExplicit, const string &name_);
    CppSourceTarget(const string &name_, Configuration *configuration_);
    CppSourceTarget(bool buildExplicit, const string &name_, Configuration *configuration_);

    CppSourceTarget(string buildCacheFilesDirPath_, const string &name_);
    CppSourceTarget(string buildCacheFilesDirPath_, bool buildExplicit, const string &name_);
    CppSourceTarget(string buildCacheFilesDirPath_, const string &name_, Configuration *configuration_);
    CppSourceTarget(string buildCacheFilesDirPath_, bool buildExplicit, const string &name_,
                    Configuration *configuration_);

    void initializeCppSourceTarget(const string &name_, string buildCacheFilesDirPath);

    void getObjectFiles(vector<const ObjectFile *> *objectFiles,
                        LinkOrArchiveTarget *linkOrArchiveTarget) const override;
    void populateTransitiveProperties();
    void adjustHeaderUnitsValueArrayPointers();
    CSourceTargetType getCSourceTargetType() const override;

    CppSourceTarget &initializeUseReqInclsFromReqIncls();
    CppSourceTarget &initializeHuDirsFromReqIncls();
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
    template <typename... U> CppSourceTarget &headerUnits(const string &headerUnit, U... headerUnitsString);
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
    template <typename T, typename... Argument>
    string GET_FLAG_evaluate(T condition, const string &flags, Argument... arguments) const;
}; // class Target

bool operator<(const CppSourceTarget &lhs, const CppSourceTarget &rhs);

template <typename... U> CppSourceTarget &CppSourceTarget::publicDeps(CppSourceTarget *dep, const U... deps)
{
    requirementDeps.emplace(dep);
    usageRequirementDeps.emplace(dep);
    addDependency<2>(*dep);
    if constexpr (sizeof...(deps))
    {
        return publicDeps(deps...);
    }
    return static_cast<CppSourceTarget &>(*this);
}

template <typename... U> CppSourceTarget &CppSourceTarget::privateDeps(CppSourceTarget *dep, const U... deps)
{
    requirementDeps.emplace(dep);
    addDependency<2>(*dep);
    if constexpr (sizeof...(deps))
    {
        return privateDeps(deps...);
    }
    return static_cast<CppSourceTarget &>(*this);
}

template <typename... U> CppSourceTarget &CppSourceTarget::interfaceDeps(CppSourceTarget *dep, const U... deps)
{
    usageRequirementDeps.emplace(dep);
    addDependency<2>(*dep);
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
        addDependency<2>(*dep);
    }
    else if (dependency == Dependency::PRIVATE)
    {
        requirementDeps.emplace(dep);
        addDependency<2>(*dep);
    }
    else
    {
        usageRequirementDeps.emplace(dep);
        addDependency<2>(*dep);
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
            actuallyAddInclude(reqHuDirs, this, include);
            actuallyAddInclude(useReqHuDirs, this, include);
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
            actuallyAddInclude(reqHuDirs, this, include);
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
            actuallyAddInclude(useReqHuDirs, this, include);
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
            actuallyAddInclude(reqHuDirs, this, include);
            actuallyAddInclude(useReqHuDirs, this, include);
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
            actuallyAddInclude(reqHuDirs, this, include);
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
            actuallyAddInclude(useReqHuDirs, this, include);
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

template <typename... U> CppSourceTarget &CppSourceTarget::headerUnits(const string &headerUnit, U... headerUnitsString)
{
    if (evaluate(UseMiniTarget::YES))
    {
        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            using namespace Indices::ConfigCache;
            Node *node = Node::getNodeFromNonNormalizedString(headerUnit, true);
            Node *inclNode = Node::getNodeFromNormalizedString(path(node->filePath).parent_path().string(), false);
            buildOrConfigCacheCopy[CppConfig::headerUnits].PushBack(node->getValue(), cacheAlloc);
            buildOrConfigCacheCopy[CppConfig::headerUnits].PushBack(inclNode->getValue(), cacheAlloc);
        }
    }

    if constexpr (sizeof...(headerUnitsString))
    {
        return sourceFiles(headerUnitsString...);
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
    if constexpr (std::is_same_v<decltype(property), CxxFlags>)
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
    // TODO
    // Will look into that

    else if constexpr (std::is_same_v<decltype(property), TranslateInclude>)
    {
        // translateInclude = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TreatModuleAsSource>)
    {
        // treatModuleAsSource = property;
    }
    // TODO
    // Will see what is this doing.
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        property;
    }
    else
    {
        static_assert(false && "Unknown feature");
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
    return configuration->compilerFeatures.evaluate(property);
}

template <typename T, typename... Argument>
string CppSourceTarget::GET_FLAG_evaluate(T condition, const string &flags, Argument... arguments) const
{
    if (evaluate(condition))
    {
        return flags;
    }
    if constexpr (sizeof...(arguments))
    {
        return GET_FLAG_evaluate(arguments...);
    }
    else
    {
        return "";
    }
}

#endif // HMAKE_CPPSOURCETARGET_HPP
