#ifndef HMAKE_CPPSOURCETARGET_HPP
#define HMAKE_CPPSOURCETARGET_HPP
#ifdef USE_HEADER_UNITS
import "BuildTools.hpp";
import "Configuration.hpp";
import "DSC.hpp";
import "HashedCommand.hpp";
import "JConsts.hpp";
import "ObjectFileProducer.hpp";
import "RunCommand.hpp";
import "SMFile.hpp";
import "ToolsCache.hpp";
import <concepts>;
import <set>;
#else
#include "BuildTools.hpp"
#include "Configuration.hpp"
#include "DSC.hpp"
#include "HashedCommand.hpp"
#include "JConsts.hpp"
#include "ObjectFileProducer.hpp"
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

struct RequireNameTargetId
{
    uint64_t id;
    string requireName;
    RequireNameTargetId(uint64_t id_, string_view requirePath_);
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
class CppSourceTarget : public ObjectFileProducerWithDS<CppSourceTarget>, public TargetCache
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
    BuildCache::Cpp cppBuildCache;

    flat_hash_set<Define> reqCompileDefinitions;
    flat_hash_set<Define> useReqCompileDefinitions;

    // Written mutex locked in round 1 updateBTarget
    flat_hash_set<SMFile *, SMFileHash, SMFileEqual> headerUnitsSet;

    vector<HuTargetPlusDir> useReqHuDirs;
    vector<HuTargetPlusDir> reqHuDirs;

    using BaseType = CSourceTarget;

    friend struct PostCompile;

    // Compile Command excluding source-file or source-files(in case of module) that is also stored in the cache.
    string compileCommand;
    string sourceCompileCommandPrintFirstHalf;
    string reqCompilerFlags;
    string useReqCompilerFlags;

    // Compile Command including tool. Tool is separated from compile command because on Windows, resource-file needs to
    // be used.
    HashedCommand compileCommandWithTool;

    vector<SourceNode> srcFileDeps;
    // Comparator used is same as for SourceNode
    vector<SMFile> modFileDeps;

    vector<SMFile> oldHeaderUnits;
    BuildCache::Cpp::ModuleFile headerUnitsCache;

    vector<InclNode> reqIncls;
    vector<InclNode> useReqIncls;

    Configuration *configuration = nullptr;

    Node *buildCacheFilesDirPathNode = nullptr;
    // reqIncludes size before populateTransitiveProperties function is called
    unsigned short reqIncSizeBeforePopulate = 0;
    unsigned short cacheUpdateCount = 0;

    atomic<uint64_t> newHeaderUnitsSize = 0;

    bool hasManuallySpecifiedHeaderUnits = false;

    bool addedInCopyJson = false;

    void setCompileCommand();
    void setSourceCompileCommandPrintFirstHalf();
    inline string &getSourceCompileCommandPrintFirstHalf();

    string getDependenciesPString() const;
    void resolveRequirePaths();
    void initializeCppBuildCache();
    static string getInfrastructureFlags(const Compiler &compiler, bool showIncludes);
    string getCompileCommandPrintSecondPart(const SourceNode &sourceNode) const;
    string getCompileCommandPrintSecondPartSMRule(const SMFile &smFile) const;
    PostCompile CompileSMFile(const SMFile &smFile);
    PostCompile updateSourceNodeBTarget(const SourceNode &sourceNode);

    PostCompile GenerateSMRulesFile(const SMFile &smFile, bool printOnlyOnError);
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override;
    void writeBuildCache(vector<char> &buffer) override;
    void checkAndCopyBuildCache();
    void writeCacheAtConfigTime(bool before);
    void readConfigCacheAtBuildTime();
    string getTarjanNodeName() const override;

    CppSourceTarget(const string &name_, Configuration *configuration_);
    CppSourceTarget(bool buildExplicit, const string &name_, Configuration *configuration_);
    CppSourceTarget(string buildCacheFilesDirPath_, const string &name_, Configuration *configuration_);
    CppSourceTarget(string buildCacheFilesDirPath_, bool buildExplicit, const string &name_,
                    Configuration *configuration_);

    void initializeCppSourceTarget(const string &name_, string buildCacheFilesDirPath);

    void getObjectFiles(vector<const ObjectFile *> *objectFiles, LOAT *loat) const override;
    void updateBuildCache(void *ptr) override;
    void populateTransitiveProperties();

    CppSourceTarget &initializeUseReqInclsFromReqIncls();
    CppSourceTarget &initializePublicHuDirsFromReqIncls();
    void actuallyAddSourceFileConfigTime(Node *node);
    void actuallyAddModuleFileConfigTime(Node *node, bool isInterface);
    void actuallyAddHeaderUnitConfigTime(Node *node);
    uint64_t actuallyAddBigHuConfigTime(const Node *node, const string &headerUnit);

    template <typename... U> CppSourceTarget &publicDeps(CppSourceTarget *dep, const U... deps);
    template <typename... U> CppSourceTarget &privateDeps(CppSourceTarget *dep, const U... deps);
    template <typename... U> CppSourceTarget &interfaceDeps(CppSourceTarget *dep, const U... deps);

    template <typename... U> CppSourceTarget &deps(CppSourceTarget *dep, DepType dependency, const U... deps);

    // TODO
    // Also provide function overload for functions like publicIncludes here and in CPT
    template <typename... U> CppSourceTarget &publicIncludes(const string &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &privateIncludes(const string &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &interfaceIncludes(const string &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &publicHUIncludes(const string &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &privateHUIncludes(const string &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &interfaceHUIncludes(const string &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &publicHUDirs(const string &include, U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &privateHUDirs(const string &include, U... includeDirectoryPString);
    template <typename... U>
    CppSourceTarget &publicHUDirsBigHu(const string &include, const string &headerUnit, const string &logicalName,
                                       U... includeDirectoryPString);
    template <typename... U>
    CppSourceTarget &privateHUDirsBigHu(const string &include, const string &headerUnit, const string &logicalName,
                                        U... includeDirectoryPString);
    template <typename... U> CppSourceTarget &interfaceHUDirs(const string &include, U... includeDirectoryPString);
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
    template <typename... U> CppSourceTarget &sourceDirs(const string &sourceDirectory, U... dirs);
    template <typename... U> CppSourceTarget &moduleDirs(const string &moduleDirectory, U... dirs);
    template <typename... U>
    CppSourceTarget &sourceDirsRE(const string &sourceDirectory, const string &regex, U... dirs);
    template <typename... U>
    CppSourceTarget &moduleDirsRE(const string &moduleDirectory, const string &regex, U... dirs);
    template <typename... U> CppSourceTarget &rSourceDirs(const string &sourceDirectory, U... dirs);
    template <typename... U> CppSourceTarget &rModuleDirs(const string &moduleDirectory, U... dirs);
    template <typename... U>
    CppSourceTarget &rSourceDirsRE(const string &sourceDirectory, const string &regex, U... dirs);
    template <typename... U>
    CppSourceTarget &rModuleDirsRE(const string &moduleDirectory, const string &regex, U... dirs);
    //
    template <DepType dependency = DepType::PRIVATE, typename T, typename... Property>
    CppSourceTarget &assign(T property, Property... properties);
    template <typename T> bool evaluate(T property) const;
    template <typename T, typename... Argument>
    string GET_FLAG_evaluate(T condition, const string &flags, Argument... arguments) const;
}; // class Target

bool operator<(const CppSourceTarget &lhs, const CppSourceTarget &rhs);

template <typename... U> CppSourceTarget &CppSourceTarget::publicDeps(CppSourceTarget *dep, const U... deps)
{
    reqDeps.emplace(dep);
    useReqDeps.emplace(dep);
    addDependencyNoMutex<2>(*dep);
    if constexpr (sizeof...(deps))
    {
        return publicDeps(deps...);
    }
    return static_cast<CppSourceTarget &>(*this);
}

template <typename... U> CppSourceTarget &CppSourceTarget::privateDeps(CppSourceTarget *dep, const U... deps)
{
    reqDeps.emplace(dep);
    addDependencyNoMutex<2>(*dep);
    if constexpr (sizeof...(deps))
    {
        return privateDeps(deps...);
    }
    return static_cast<CppSourceTarget &>(*this);
}

template <typename... U> CppSourceTarget &CppSourceTarget::interfaceDeps(CppSourceTarget *dep, const U... deps)
{
    useReqDeps.emplace(dep);
    addDependencyNoMutex<2>(*dep);
    if constexpr (sizeof...(deps))
    {
        return interfaceDeps(deps...);
    }
    return static_cast<CppSourceTarget &>(*this);
}

template <typename... U>
CppSourceTarget &CppSourceTarget::deps(CppSourceTarget *dep, const DepType dependency, const U... cppSourceTargets)
{
    if (dependency == DepType::PUBLIC)
    {
        reqDeps.emplace(dep);
        useReqDeps.emplace(dep);
        addDependencyNoMutex<2>(*dep);
    }
    else if (dependency == DepType::PRIVATE)
    {
        reqDeps.emplace(dep);
        addDependencyNoMutex<2>(*dep);
    }
    else
    {
        useReqDeps.emplace(dep);
        addDependencyNoMutex<2>(*dep);
    }
    if constexpr (sizeof...(cppSourceTargets))
    {
        return deps(cppSourceTargets...);
    }
    return static_cast<CppSourceTarget &>(*this);
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicIncludes(const string &include, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
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
    if constexpr (bsMode == BSMode::CONFIGURE)
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
    if constexpr (bsMode == BSMode::CONFIGURE)
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
    if constexpr (bsMode == BSMode::CONFIGURE)
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
    if constexpr (bsMode == BSMode::CONFIGURE)
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
    if constexpr (bsMode == BSMode::CONFIGURE)
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
CppSourceTarget &CppSourceTarget::publicHUDirs(const string &include, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (evaluate(TreatModuleAsSource::NO))
        {
            actuallyAddInclude(reqHuDirs, this, include);
            actuallyAddInclude(useReqHuDirs, this, include);
        }
    }

    if constexpr (sizeof...(includeDirectoryPString))
    {
        return publicHUDirs(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateHUDirs(const string &include, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (evaluate(TreatModuleAsSource::NO))
        {
            actuallyAddInclude(reqHuDirs, this, include);
        }
    }

    if constexpr (sizeof...(includeDirectoryPString))
    {
        return privateHUDirs(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicHUDirsBigHu(const string &include, const string &headerUnit,
                                                    const string &logicalName, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (evaluate(TreatModuleAsSource::NO))
        {
            uint64_t headerUnitsIndex =
                actuallyAddBigHuConfigTime(Node::getNodeFromNonNormalizedString(headerUnit, true), logicalName);
            actuallyAddInclude(reqHuDirs, this, include, cacheIndex, headerUnitsIndex);
            actuallyAddInclude(useReqHuDirs, this, include, cacheIndex, headerUnitsIndex);
        }
    }

    if constexpr (sizeof...(includeDirectoryPString))
    {
        return publicHUDirsBigHu(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateHUDirsBigHu(const string &include, const string &headerUnit,
                                                     const string &logicalName, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (evaluate(TreatModuleAsSource::NO))
        {
            uint64_t headerUnitsIndex =
                actuallyAddBigHuConfigTime(Node::getNodeFromNonNormalizedString(headerUnit, true), logicalName);
            actuallyAddInclude(reqHuDirs, this, include, cacheIndex, headerUnitsIndex);
        }
    }

    if constexpr (sizeof...(includeDirectoryPString))
    {
        return privateHUDirsBigHu(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceHUDirs(const string &include, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (evaluate(TreatModuleAsSource::NO))
        {
            actuallyAddInclude(useReqHuDirs, this, include);
        }
    }

    if constexpr (sizeof...(includeDirectoryPString))
    {
        return interfaceHUDirs(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppSourceTarget &CppSourceTarget::sourceFiles(const string &srcFile, U... sourceFilePString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        actuallyAddSourceFileConfigTime(Node::getNodeFromNonNormalizedString(srcFile, true));
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
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (evaluate(TreatModuleAsSource::YES))
        {
            return sourceFiles(modFile, moduleFilePString...);
        }
        actuallyAddModuleFileConfigTime(Node::getNodeFromNonNormalizedString(modFile, true), false);
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
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (evaluate(TreatModuleAsSource::YES))
        {
            return sourceFiles(modFile, moduleFilePString...);
        }
        actuallyAddModuleFileConfigTime(Node::getNodeFromNonNormalizedString(modFile, true), true);
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
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        actuallyAddHeaderUnitConfigTime(Node::getNodeFromNonNormalizedString(headerUnit, true));
    }

    if constexpr (sizeof...(headerUnitsString))
    {
        return headerUnits(headerUnitsString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppSourceTarget &CppSourceTarget::sourceDirs(const string &sourceDirectory, U... dirs)
{
    parseRegexSourceDirs(true, sourceDirectory, ".*", false);
    if constexpr (sizeof...(dirs))
    {
        return sourceDirs(dirs...);
    }
    return *this;
}

template <typename... U> CppSourceTarget &CppSourceTarget::moduleDirs(const string &moduleDirectory, U... dirs)
{
    parseRegexSourceDirs(false, moduleDirectory, ".*", false);
    if constexpr (sizeof...(dirs))
    {
        return moduleDirs(dirs...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::sourceDirsRE(const string &sourceDirectory, const string &regex, U... dirs)
{
    parseRegexSourceDirs(true, sourceDirectory, regex, false);
    if constexpr (sizeof...(dirs))
    {
        return sourceDirsRE(dirs...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::moduleDirsRE(const string &moduleDirectory, const string &regex, U... dirs)
{
    parseRegexSourceDirs(false, moduleDirectory, regex, false);
    if constexpr (sizeof...(dirs))
    {
        return moduleDirsRE(dirs...);
    }
    return *this;
}

template <typename... U> CppSourceTarget &CppSourceTarget::rSourceDirs(const string &sourceDirectory, U... dirs)
{
    parseRegexSourceDirs(true, sourceDirectory, ".*", true);
    if constexpr (sizeof...(dirs))
    {
        return rSourceDirs(dirs...);
    }
    return *this;
}

template <typename... U> CppSourceTarget &CppSourceTarget::rModuleDirs(const string &moduleDirectory, U... dirs)
{
    parseRegexSourceDirs(false, moduleDirectory, ".*", true);
    if constexpr (sizeof...(dirs))
    {
        return rModuleDirs(dirs...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::rSourceDirsRE(const string &sourceDirectory, const string &regex, U... dirs)
{
    parseRegexSourceDirs(true, sourceDirectory, regex, true);
    if constexpr (sizeof...(dirs))
    {
        return R_sourceDirsRE(dirs...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::rModuleDirsRE(const string &moduleDirectory, const string &regex, U... dirs)
{
    parseRegexSourceDirs(false, moduleDirectory, regex, true);
    if constexpr (sizeof...(dirs))
    {
        return R_moduleDirsRE(dirs...);
    }
    return *this;
}

template <DepType dependency, typename T, typename... Property>
CppSourceTarget &CppSourceTarget::assign(T property, Property... properties)
{
    if constexpr (std::is_same_v<decltype(property), CxxFlags>)
    {
        if constexpr (dependency == DepType::PRIVATE)
        {
            reqCompilerFlags += property;
        }
        else if constexpr (dependency == DepType::INTERFACE)
        {
            useReqCompilerFlags += property;
        }
        else
        {
            reqCompilerFlags += property;
            useReqCompilerFlags += property;
        }
    }
    else if constexpr (std::is_same_v<decltype(property), Define>)
    {
        if constexpr (dependency == DepType::PRIVATE)
        {
            reqCompileDefinitions.emplace(property);
        }
        else if constexpr (dependency == DepType::INTERFACE)
        {
            useReqCompileDefinitions.emplace(property);
        }
        else
        {
            reqCompileDefinitions.emplace(property);
            useReqCompileDefinitions.emplace(property);
        }
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        property;
    }
    else
    {
        static_assert(false && "Unknown feature");
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

template <> DSC<CppSourceTarget>::DSC(CppSourceTarget *ptr, PLOAT *ploat_, bool defines, string define_);

template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::save(CppSourceTarget &ptr);
template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::saveAndReplace(CppSourceTarget &ptr);
template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::restore();

// TODO
// Optimize this
inline vector<CppSourceTarget *> cppSourceTargets{10000};

#endif // HMAKE_CPPSOURCETARGET_HPP
