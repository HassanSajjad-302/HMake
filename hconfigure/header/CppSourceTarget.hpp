#ifndef HMAKE_CPPSOURCETARGET_HPP
#define HMAKE_CPPSOURCETARGET_HPP

#include "BuildTools.hpp"
#include "Configuration.hpp"
#include "DSC.hpp"
#include "HashedCommand.hpp"
#include "ObjectFileProducer.hpp"
#include "SMFile.hpp"
#include <concepts>

using std::same_as;

struct HeaderFileOrUnit
{
    union {
        SMFile *smFile = nullptr;
        Node *node;
    } data;
    bool isUnit;
    bool isSystem;
    HeaderFileOrUnit(SMFile *smFile_, bool isSystem_);
    HeaderFileOrUnit(Node *node_, bool isSystem_);
    HeaderFileOrUnit() = default;
};

enum class FileType : uint8_t
{
    MODULE,
    HEADER_FILE,
    HEADER_UNIT,
};

// TODO
// HMake currently does not has proper C Support. There is workaround by ASSING(CSourceTargetEnum::YES) call which that
// use -TC flag with MSVC
class CppSourceTarget : public ObjectFileProducerWithDS<CppSourceTarget>, public TargetCache
{
  public:
    BuildCache::Cpp cppBuildCache;

    flat_hash_set<Define> reqCompileDefinitions;
    flat_hash_set<Define> useReqCompileDefinitions;
    flat_hash_map<string, SMFile *> imodNames;

    using BaseType = CSourceTarget;

    // Compile Command excluding source-file or source-files(in case of module) that is also stored in the cache.
    string compileCommand;
    string reqCompilerFlags;
    string useReqCompilerFlags;

    // Compile Command including tool. Tool is separated from compile command because on Windows, resource-file needs to
    // be used.
    HashedCommand compileCommandWithTool;

    vector<SourceNode *> srcFileDeps;
    vector<SMFile *> modFileDeps;
    vector<SMFile *> imodFileDeps;
    vector<SMFile *> huDeps;
    BuildCache::Cpp::ModuleFile headerUnitsCache;

    vector<InclNode> reqIncls;
    vector<InclNode> useReqIncls;

    flat_hash_map<string_view, HeaderFileOrUnit> reqHeaderNameMapping;
    flat_hash_map<string_view, HeaderFileOrUnit> useReqHeaderNameMapping;

    flat_hash_map<const Node *, FileType> reqNodesType;
    flat_hash_map<const Node *, FileType> useReqNodesType;

    Configuration *configuration = nullptr;

    Node *myBuildDir = nullptr;
    // reqIncludes size before populateTransitiveProperties function is called
    unsigned short reqIncSizeBeforePopulate = 0;
    unsigned short cacheUpdateCount = 0;

    atomic<uint64_t> newHeaderUnitsSize = 0;

    // Used only at configure time
    vector<SMFile *> publicBigHu;
    vector<SMFile *> privateBigHu;
    vector<SMFile *> interfaceBigHu;

    // Used only at configure time
    uint32_t reqHeaderFilesSize = 0;
    uint32_t useReqHeaderFilesSize = 0;

    bool isSystem = false;
    bool ignoreHeaderDeps = false;

    void setCompileCommand();

    string getDependenciesString() const;
    static string getInfrastructureFlags(const Compiler &compiler);
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override;
    void writeBuildCache(vector<char> &buffer) override;
    void setHeaderStatusChanged(BuildCache::Cpp::ModuleFile &modCache);
    void writeBigHeaderUnits();
    void writeCacheAtConfigTime();
    void readConfigCacheAtBuildTime();
    string getPrintName() const override;
    BTargetType getBTargetType() const override;

    CppSourceTarget(const string &name_, Configuration *configuration_);
    CppSourceTarget(bool buildExplicit, const string &name_, Configuration *configuration_);
    CppSourceTarget(string buildCacheFilesDirPath_, const string &name_, Configuration *configuration_);
    CppSourceTarget(string buildCacheFilesDirPath_, bool buildExplicit, const string &name_,
                    Configuration *configuration_);

    void initializeCppSourceTarget(const string &name_, string buildCacheFilesDirPath);

    void getObjectFiles(vector<const ObjectFile *> *objectFiles, LOAT *loat) const override;
    void updateBuildCache(void *ptr, string &outputStr, string &errorStr, bool &buildCacheModified) override;
    void populateTransitiveProperties();

    void actuallyAddSourceFileConfigTime(Node *node);
    void actuallyAddModuleFileConfigTime(Node *node, string exportName);
    void emplaceInHeaderNameMapping(string_view headerName, HeaderFileOrUnit type, bool addInReq, bool suppressError);
    void emplaceInNodesType(const Node *node, FileType type, bool addInReq);
    void makeHeaderFileAsUnit(const string &logicalName, bool addInReq, bool addInUseReq);
    void removeHeaderFile(const string &logicalName, bool addInReq, bool addInUseReq);
    void removeHeaderUnit(const Node *headerNode, const string &logicalName, bool addInReq, bool addInUseReq);
    void addHeaderFile(const string &logicalName, const Node *headerFile, bool suppressError, bool addInReq,
                       bool addInUseReq);
    void addHeaderUnit(const string &logicalName, const Node *headerUnit, bool suppressError, bool addInReq,
                       bool addInUseReq);
    void addHeaderUnitOrFileDir(const Node *includeDir, const string &prefix, bool isHeaderFile, const string &regexStr,
                                bool addInReq, bool addInUseReq);
    void addHeaderUnitOrFileDirMSVC(const Node *includeDir, bool isHeaderFile, bool useMentioned, bool addInReq,
                                    bool addInUseReq, bool isStandard, bool ignoreHeaderDeps);
    void actuallyAddInclude(bool errorOnEmplaceFail, const Node *include, bool addInReq, bool addInUseReq);
    void readModuleMapFromDir(const string &dir);

    template <typename... U> CppSourceTarget &publicDeps(CppSourceTarget *dep, const U... deps);
    template <typename... U> CppSourceTarget &privateDeps(CppSourceTarget *dep, const U... deps);
    template <typename... U> CppSourceTarget &interfaceDeps(CppSourceTarget *dep, const U... deps);

    template <typename... U> CppSourceTarget &deps(CppSourceTarget *dep, DepType dependency, const U... deps);

    template <typename... U> CppSourceTarget &moduleMaps(const string &include, U... includeDirectoryString);
    template <typename... U> CppSourceTarget &publicIncludes(const string &include, U... includeDirectoryString);
    template <typename... U> CppSourceTarget &privateIncludes(const string &include, U... includeDirectoryString);
    template <typename... U> CppSourceTarget &interfaceIncludes(const string &include, U... includeDirectoryString);
    template <typename... U> CppSourceTarget &publicHUIncludes(const string &include, U... includeDirectoryString);
    template <typename... U> CppSourceTarget &privateHUIncludes(const string &include, U... includeDirectoryString);
    template <typename... U> CppSourceTarget &interfaceHUIncludes(const string &include, U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &publicIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &privateIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &interfaceIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &publicHUIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &privateHUIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &interfaceHUIncludesRE(const string &include, const string &regexStr, U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &publicHUDirs(const string &include, const string &prefix, U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &privateHUDirs(const string &include, const string &prefix, U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &interfaceHUDirs(const string &include, const string &prefix, U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &publicHUDirsRE(const string &include, const string &prefix, const string &regexStr,
                                    U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &privateHUDirsRE(const string &include, const string &prefix, const string &regexStr,
                                     U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &interfaceHUDirsRE(const string &include, const string &prefix, const string &regexStr,
                                       U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &publicIncDirs(const string &include, const string &prefix, U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &privateIncDirs(const string &include, const string &prefix, U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &interfaceIncDirs(const string &include, const string &prefix, U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &publicIncDirsRE(const string &include, const string &prefix, const string &regexStr,
                                     U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &privateIncDirsRE(const string &include, const string &prefix, const string &regexStr,
                                      U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &interfaceIncDirsRE(const string &include, const string &prefix, const string &regexStr,
                                        U... includeDirectoryString);
    template <typename... U> CppSourceTarget &publicIncludesSource(const string &include, U... includeDirectoryString);
    template <typename... U> CppSourceTarget &privateIncludesSource(const string &include, U... includeDirectoryString);
    template <typename... U>
    CppSourceTarget &interfaceIncludesSource(const string &include, U... includeDirectoryString);
    CppSourceTarget &publicCompilerFlags(const string &compilerFlags);
    CppSourceTarget &privateCompilerFlags(const string &compilerFlags);
    CppSourceTarget &interfaceCompilerFlags(const string &compilerFlags);

    template <typename... U>
    CppSourceTarget &publicCompileDefines(const string &cddName, const string &cddValue, U... compileDefines);
    template <typename... U>
    CppSourceTarget &privateCompileDefines(const string &cddName, const string &cddValue, U... compileDefines);
    template <typename... U>
    CppSourceTarget &interfaceCompileDefines(const string &cddName, const string &cddValue, U... compileDefines);

    template <typename... U>
    CppSourceTarget &interfaceFiles(const string &modFile, const string &exportName, U... moduleFileString);
    template <typename... U>
    CppSourceTarget &publicHeaderFiles(const string &logicalName, const string &headerFile, U... headerUnitsString);
    template <typename... U>
    CppSourceTarget &privateHeaderFiles(const string &logicalName, const string &headerFile, U... headerUnitsString);
    template <typename... U>
    CppSourceTarget &interfaceHeaderFiles(const string &logicalName, const string &headerFile, U... headerUnitsString);
    template <typename... U>
    CppSourceTarget &publicHeaderUnits(const string &logicalName, const string &headerUnit, U... headerUnitsString);
    template <typename... U>
    CppSourceTarget &privateHeaderUnits(const string &logicalName, const string &headerUnit, U... headerUnitsString);
    template <typename... U>
    CppSourceTarget &interfaceHeaderUnits(const string &logicalName, const string &headerUnit, U... headerUnitsString);
    void parseRegexSourceDirs(bool assignToSourceNodes, const string &sourceDirectory, string regexStr, bool recursive);
    template <typename... U> CppSourceTarget &sourceFiles(const string &srcFile, U... sourceFileString);
    template <typename... U> CppSourceTarget &sourceDirs(const string &sourceDirectory, U... dirs);
    template <typename... U>
    CppSourceTarget &sourceDirsRE(const string &sourceDirectory, const string &regex, U... dirs);
    template <typename... U> CppSourceTarget &rSourceDirs(const string &sourceDirectory, U... dirs);
    template <typename... U>
    CppSourceTarget &rSourceDirsRE(const string &sourceDirectory, const string &regex, U... dirs);
    template <typename... U> CppSourceTarget &moduleFiles(const string &modFile, U... moduleFileString);
    template <typename... U> CppSourceTarget &moduleDirs(const string &moduleDirectory, U... dirs);
    template <typename... U>
    CppSourceTarget &moduleDirsRE(const string &moduleDirectory, const string &regex, U... dirs);
    template <typename... U> CppSourceTarget &rModuleDirs(const string &moduleDirectory, U... dirs);
    template <typename... U>
    CppSourceTarget &rModuleDirsRE(const string &moduleDirectory, const string &regex, U... dirs);
    //
    template <DepType dependency = DepType::PRIVATE, typename T, typename... Property>
    CppSourceTarget &assign(T property, Property... properties);
}; // class Target

bool operator<(const CppSourceTarget &lhs, const CppSourceTarget &rhs);

template <typename... U> CppSourceTarget &CppSourceTarget::publicDeps(CppSourceTarget *dep, const U... deps)
{
    reqDeps.emplace(dep);
    useReqDeps.emplace(dep);
    addDepNow<1>(*dep);
    if constexpr (sizeof...(deps))
    {
        return publicDeps(deps...);
    }
    return static_cast<CppSourceTarget &>(*this);
}

template <typename... U> CppSourceTarget &CppSourceTarget::privateDeps(CppSourceTarget *dep, const U... deps)
{
    reqDeps.emplace(dep);
    addDepNow<1>(*dep);
    if constexpr (sizeof...(deps))
    {
        return privateDeps(deps...);
    }
    return static_cast<CppSourceTarget &>(*this);
}

template <typename... U> CppSourceTarget &CppSourceTarget::interfaceDeps(CppSourceTarget *dep, const U... deps)
{
    useReqDeps.emplace(dep);
    addDepNow<1>(*dep);
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
        addDepNow<1>(*dep);
    }
    else if (dependency == DepType::PRIVATE)
    {
        reqDeps.emplace(dep);
        addDepNow<1>(*dep);
    }
    else
    {
        useReqDeps.emplace(dep);
        addDepNow<1>(*dep);
    }
    if constexpr (sizeof...(cppSourceTargets))
    {
        return deps(cppSourceTargets...);
    }
    return static_cast<CppSourceTarget &>(*this);
}

template <typename... U>
CppSourceTarget &CppSourceTarget::moduleMaps(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        readModuleMapFromDir(include);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return moduleMaps(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicIncludes(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
        actuallyAddInclude(true, inclNode, true, true);
        addHeaderUnitOrFileDir(inclNode, "", true, "", true, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicIncludes(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateIncludes(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
        actuallyAddInclude(true, inclNode, true, false);
        addHeaderUnitOrFileDir(inclNode, "", true, "", true, false);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateIncludes(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceIncludes(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
        actuallyAddInclude(true, inclNode, false, true);
        addHeaderUnitOrFileDir(inclNode, "", true, "", false, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceIncludes(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicHUIncludes(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
        actuallyAddInclude(true, inclNode, true, true);
        addHeaderUnitOrFileDir(inclNode, "", false, "", true, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicHUIncludes(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateHUIncludes(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
        actuallyAddInclude(true, inclNode, true, false);
        addHeaderUnitOrFileDir(inclNode, "", false, "", true, false);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateHUIncludes(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceHUIncludes(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
        actuallyAddInclude(true, inclNode, false, true);
        addHeaderUnitOrFileDir(inclNode, "", false, "", false, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceHUIncludes(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicIncludesRE(const string &include, const string &regexStr,
                                                   U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
        actuallyAddInclude(false, inclNode, true, true);
        addHeaderUnitOrFileDir(inclNode, "", true, regexStr, true, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicIncludesRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateIncludesRE(const string &include, const string &regexStr,
                                                    U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
        actuallyAddInclude(false, inclNode, true, false);
        addHeaderUnitOrFileDir(inclNode, "", true, regexStr, true, false);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateIncludesRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceIncludesRE(const string &include, const string &regexStr,
                                                      U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
        actuallyAddInclude(false, inclNode, false, true);
        addHeaderUnitOrFileDir(inclNode, "", true, regexStr, false, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceIncludesRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicHUIncludesRE(const string &include, const string &regexStr,
                                                     U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
        actuallyAddInclude(false, inclNode, true, true);
        addHeaderUnitOrFileDir(inclNode, "", false, regexStr, true, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicHUIncludesRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateHUIncludesRE(const string &include, const string &regexStr,
                                                      U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
        actuallyAddInclude(false, inclNode, true, false);
        addHeaderUnitOrFileDir(inclNode, "", false, regexStr, true, false);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateHUIncludesRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceHUIncludesRE(const string &include, const string &regexStr,
                                                        U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
        actuallyAddInclude(false, inclNode, false, true);
        addHeaderUnitOrFileDir(inclNode, "", false, regexStr, false, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceHUIncludesRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicHUDirs(const string &include, const string &prefix, U... includeDirectoryString)
{

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, false, "", true, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicHUDirs(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateHUDirs(const string &include, const string &prefix,
                                                U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, false, "", true, false);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateHUDirs(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceHUDirs(const string &include, const string &prefix,
                                                  U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, false, "", false, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceHUDirs(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicHUDirsRE(const string &include, const string &prefix, const string &regexStr,
                                                 U... includeDirectoryString)
{

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, false, regexStr, true, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicHUDirsRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateHUDirsRE(const string &include, const string &prefix, const string &regexStr,
                                                  U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, false, regexStr, true, false);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateHUDirsRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceHUDirsRE(const string &include, const string &prefix, const string &regexStr,
                                                    U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, false, regexStr, false, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceHUDirsRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicIncDirs(const string &include, const string &prefix,
                                                U... includeDirectoryString)
{

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, true, "", true, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicIncDirs(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateIncDirs(const string &include, const string &prefix,
                                                 U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, true, "", true, false);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateIncDirs(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceIncDirs(const string &include, const string &prefix,
                                                   U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, true, "", false, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceIncDirs(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicIncDirsRE(const string &include, const string &prefix, const string &regexStr,
                                                  U... includeDirectoryString)
{

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, true, regexStr, true, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicIncDirsRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateIncDirsRE(const string &include, const string &prefix, const string &regexStr,
                                                   U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, true, regexStr, true, false);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateIncDirsRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceIncDirsRE(const string &include, const string &prefix,
                                                     const string &regexStr, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
            addHeaderUnitOrFileDir(inclNode, prefix, true, regexStr, false, true);
        }
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceIncDirsRE(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicIncludesSource(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
        actuallyAddInclude(true, inclNode, true, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return publicIncludesSource(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateIncludesSource(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
        actuallyAddInclude(true, inclNode, true, false);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return privateIncludesSource(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceIncludesSource(const string &include, U... includeDirectoryString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *inclNode = Node::getNodeFromNonNormalizedPath(include, false);
        actuallyAddInclude(true, inclNode, false, true);
    }

    if constexpr (sizeof...(includeDirectoryString))
    {
        return interfaceIncludesSource(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppSourceTarget &CppSourceTarget::sourceFiles(const string &srcFile, U... sourceFileString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        actuallyAddSourceFileConfigTime(Node::getNodeFromNonNormalizedString(srcFile, true));
    }

    if constexpr (sizeof...(sourceFileString))
    {
        return sourceFiles(sourceFileString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppSourceTarget &CppSourceTarget::moduleFiles(const string &modFile, U... moduleFileString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::YES))
        {
            return sourceFiles(modFile, moduleFileString...);
        }
        actuallyAddModuleFileConfigTime(Node::getNodeFromNonNormalizedString(modFile, true), "");
    }

    if constexpr (sizeof...(moduleFileString))
    {
        return moduleFiles(moduleFileString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicCompileDefines(const string &cddName, const string &cddValue,
                                                       U... compileDefines)
{
    reqCompileDefinitions.emplace(cddName, cddValue);
    useReqCompileDefinitions.emplace(cddName, cddValue);

    if constexpr (sizeof...(compileDefines))
    {
        return publicCompileDefines(compileDefines...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateCompileDefines(const string &cddName, const string &cddValue,
                                                        U... compileDefines)
{
    reqCompileDefinitions.emplace(cddName, cddValue);

    if constexpr (sizeof...(compileDefines))
    {
        return privateCompileDefines(compileDefines...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceCompileDefines(const string &cddName, const string &cddValue,
                                                          U... compileDefines)
{
    useReqCompileDefinitions.emplace(cddName, cddValue);

    if constexpr (sizeof...(compileDefines))
    {
        return interfaceCompileDefines(compileDefines...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceFiles(const string &modFile, const string &exportName, U... moduleFileString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        actuallyAddModuleFileConfigTime(Node::getNodeFromNonNormalizedString(modFile, true), exportName);
    }

    if constexpr (sizeof...(moduleFileString))
    {
        return interfaceFiles(moduleFileString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicHeaderFiles(const string &logicalName, const string &headerFile,
                                                    U... headersString)
{

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            addHeaderFile(logicalName, Node::getNodeFromNonNormalizedString(headerFile, true), false, true, true);
        }
    }

    if constexpr (sizeof...(headersString))
    {
        return publicHeaderFiles(headersString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateHeaderFiles(const string &logicalName, const string &headerFile,
                                                     U... headersString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            addHeaderFile(logicalName, Node::getNodeFromNonNormalizedString(headerFile, true), false, true, false);
        }
    }

    if constexpr (sizeof...(headersString))
    {
        return privateHeaderFiles(headersString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceHeaderFiles(const string &logicalName, const string &headerFile,
                                                       U... headersString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            addHeaderFile(logicalName, Node::getNodeFromNonNormalizedString(headerFile, true), false, false, true);
        }
    }

    if constexpr (sizeof...(headersString))
    {
        return interfaceHeaderFiles(headersString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::publicHeaderUnits(const string &logicalName, const string &headerUnit,
                                                    U... headersString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            addHeaderUnit(logicalName, Node::getNodeFromNonNormalizedString(headerUnit, true), false, true, true);
        }
    }

    if constexpr (sizeof...(headersString))
    {
        return publicHeaderUnits(headersString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::privateHeaderUnits(const string &logicalName, const string &headerUnit,
                                                     U... headersString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            addHeaderUnit(logicalName, Node::getNodeFromNonNormalizedString(headerUnit, true), false, true, false);
        }
    }

    if constexpr (sizeof...(headersString))
    {
        return privateHeaderUnits(headersString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U>
CppSourceTarget &CppSourceTarget::interfaceHeaderUnits(const string &logicalName, const string &headerUnit,
                                                       U... headersString)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::NO))
        {
            addHeaderUnit(logicalName, Node::getNodeFromNonNormalizedString(headerUnit, true), false, false, true);
        }
    }

    if constexpr (sizeof...(headersString))
    {
        return interfaceHeaderUnits(headersString...);
    }
    else
    {
        return *this;
    }
}

template <typename... U> CppSourceTarget &CppSourceTarget::sourceDirs(const string &sourceDirectory, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(true, sourceDirectory, ".*", false);
    }
    if constexpr (sizeof...(dirs))
    {
        return sourceDirs(dirs...);
    }
    return *this;
}

template <typename... U> CppSourceTarget &CppSourceTarget::moduleDirs(const string &moduleDirectory, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(false, moduleDirectory, ".*", false);
    }
    if constexpr (sizeof...(dirs))
    {
        return moduleDirs(dirs...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::sourceDirsRE(const string &sourceDirectory, const string &regex, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(true, sourceDirectory, regex, false);
    }
    if constexpr (sizeof...(dirs))
    {
        return sourceDirsRE(dirs...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::moduleDirsRE(const string &moduleDirectory, const string &regex, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(false, moduleDirectory, regex, false);
    }
    if constexpr (sizeof...(dirs))
    {
        return moduleDirsRE(dirs...);
    }
    return *this;
}

template <typename... U> CppSourceTarget &CppSourceTarget::rSourceDirs(const string &sourceDirectory, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(true, sourceDirectory, ".*", true);
    }
    if constexpr (sizeof...(dirs))
    {
        return rSourceDirs(dirs...);
    }
    return *this;
}

template <typename... U> CppSourceTarget &CppSourceTarget::rModuleDirs(const string &moduleDirectory, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(false, moduleDirectory, ".*", true);
    }
    if constexpr (sizeof...(dirs))
    {
        return rModuleDirs(dirs...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::rSourceDirsRE(const string &sourceDirectory, const string &regex, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(true, sourceDirectory, regex, true);
    }
    if constexpr (sizeof...(dirs))
    {
        return R_sourceDirsRE(dirs...);
    }
    return *this;
}

template <typename... U>
CppSourceTarget &CppSourceTarget::rModuleDirsRE(const string &moduleDirectory, const string &regex, U... dirs)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        parseRegexSourceDirs(false, moduleDirectory, regex, true);
    }
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

template <> DSC<CppSourceTarget>::DSC(CppSourceTarget *ptr, PLOAT *ploat_, bool defines, string define_);

template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::save(CppSourceTarget &ptr);
template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::saveAndReplace(CppSourceTarget &ptr);
template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::restore();

// TODO
// Optimize this
inline vector<CppSourceTarget *> cppSourceTargets{10000};

#endif // HMAKE_CPPSOURCETARGET_HPP
