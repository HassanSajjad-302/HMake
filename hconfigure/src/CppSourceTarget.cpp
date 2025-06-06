
#ifdef USE_HEADER_UNITS
import "CppSourceTarget.hpp";
import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "ConfigHelpers.hpp";
import "Configuration.hpp";
import "LOAT.hpp";
import "rapidhash.h";
import "TargetCacheDiskWriteManager.hpp";
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
#include "Configuration.hpp"
#include "LOAT.hpp"
#include "TargetCacheDiskWriteManager.hpp"
#include "Utilities.hpp"
#include "rapidhash/rapidhash.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <utility>
#endif

using std::filesystem::create_directories, std::filesystem::directory_iterator,
    std::filesystem::recursive_directory_iterator, std::ifstream, std::ofstream, std::regex, std::regex_error;

SourceDirectory::SourceDirectory(const string &sourceDirectory_, string regex_, const bool recursive_)
    : sourceDirectory{Node::getNodeFromNonNormalizedPath(sourceDirectory_, false)}, regex{std::move(regex_)},
      recursive(recursive_)
{
}

bool InclNodePointerComparator::operator()(const InclNode &lhs, const InclNode &rhs) const
{
    return lhs.node < rhs.node;
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

string ResolveRequirePathBTarget::getTarjanNodeName() const
{
    return "ResolveRequirePath " + target->name;
}

RequireNameTargetId::RequireNameTargetId(const uint64_t id_, string requirePath_)
    : id(id_), requireName(std::move(requirePath_))
{
}

bool RequireNameTargetId::operator==(const RequireNameTargetId &other) const
{
    return id == other.id && requireName == other.requireName;
}

uint64_t RequireNameTargetIdHash::operator()(const RequireNameTargetId &req) const
{
    const uint64_t hash[2] = {rapidhash(req.requireName.c_str(), req.requireName.size()), req.id};
    return rapidhash(&hash, sizeof(hash));
}

bool CppSourceTarget::SMFileEqual::operator()(const SMFile *lhs, const SMFile *rhs) const
{
    return lhs->node == rhs->node;
}

bool CppSourceTarget::SMFileEqual::operator()(const SMFile *lhs, const Node *rhs) const
{
    return lhs->node == rhs;
}

bool CppSourceTarget::SMFileEqual::operator()(const Node *lhs, const SMFile *rhs) const
{
    return lhs == rhs->node;
}

std::size_t CppSourceTarget::SMFileHash::operator()(const SMFile *node) const
{
    return reinterpret_cast<uint64_t>(node->node);
}

std::size_t CppSourceTarget::SMFileHash::operator()(const Node *node) const
{
    return reinterpret_cast<uint64_t>(node);
}

CppSourceTarget::CppSourceTarget(const string &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, false, false), TargetCache(name_), configuration(configuration_)
{
    initializeCppSourceTarget(name_, "");
}

CppSourceTarget::CppSourceTarget(const bool buildExplicit, const string &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, buildExplicit, false), TargetCache(name_), configuration(configuration_)
{
    initializeCppSourceTarget(name_, "");
}

CppSourceTarget::CppSourceTarget(string buildCacheFilesDirPath_, const string &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, false, false), TargetCache(name_), configuration(configuration_)
{
    initializeCppSourceTarget(name_, configureNode->filePath + slashc + std::move(buildCacheFilesDirPath_));
}

CppSourceTarget::CppSourceTarget(string buildCacheFilesDirPath_, const bool buildExplicit, const string &name_,
                                 Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, buildExplicit, false), TargetCache(name_), configuration(configuration_)
{
    initializeCppSourceTarget(name_, configureNode->filePath + slashc + std::move(buildCacheFilesDirPath_));
}

void CppSourceTarget::initializeCppSourceTarget(const string &name_, string buildCacheFilesDirPath)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        buildOrConfigCacheCopy.PushBack(kArrayType, cacheAlloc)
            .PushBack(kArrayType, cacheAlloc)
            .PushBack(kArrayType, cacheAlloc)
            .PushBack(kArrayType, cacheAlloc)
            .PushBack(kArrayType, cacheAlloc)
            .PushBack(kArrayType, cacheAlloc)
            .PushBack(kArrayType, cacheAlloc)
            .PushBack(Node::getType(), cacheAlloc);

        if (buildCacheFilesDirPath.empty())
        {
            buildCacheFilesDirPath = configureNode->filePath + slashc + name;
        }
        create_directories(buildCacheFilesDirPath);
        buildCacheFilesDirPathNode = Node::addHalfNodeFromNormalizedStringSingleThreaded(buildCacheFilesDirPath);
        buildOrConfigCacheCopy[Indices::ConfigCache::CppConfig::buildCacheFilesDirPath] =
            buildCacheFilesDirPathNode->getValue();

        namespace CppBuild = Indices::BuildCache::CppBuild;
        if (Value &targetBuildCache = getBuildCache(); targetBuildCache.Empty())
        {
            // Maybe store pointers to these in CppSourceTarget
            targetBuildCache.PushBack(Value(kArrayType), cacheAlloc);
            targetBuildCache[CppBuild::sourceFiles].Reserve(srcFileDeps.size(), cacheAlloc);

            targetBuildCache.PushBack(Value(kArrayType), cacheAlloc);
            targetBuildCache[CppBuild::moduleFiles].Reserve(modFileDeps.size(), cacheAlloc);

            // Header-units size can't be known as these are dynamically discovered.
            targetBuildCache.PushBack(Value(kArrayType), cacheAlloc);
        }
    }

    if constexpr (bsMode == BSMode::BUILD)
    {
        namespace CppConfig = Indices::ConfigCache::CppConfig;
        cppSourceTargets[targetCacheIndex] = this;
        Value &sourceNodesCache = getConfigCache()[CppConfig::sourceFiles];
        Value &moduleNodesCache = getConfigCache()[CppConfig::moduleFiles];
        Value &headerUnitsNodesCache = getConfigCache()[CppConfig::headerUnits];

        srcFileDeps.reserve(sourceNodesCache.Size());
        for (Value &value : sourceNodesCache.GetArray())
        {
            // ensureSystemCheckCalled is called in SourceNode::updateBTarget(0) in parallel.
            srcFileDeps.emplace_back(this, Node::getHalfNodeFromValue(value));
        }

        modFileDeps.reserve(moduleNodesCache.Size() / 2);
        for (uint64_t i = 0; i < moduleNodesCache.Size(); i = i + 2)
        {
            // ensureSystemCheckCalled is called in SMFile::updateBTarget(1) in parallel.
            modFileDeps.emplace_back(this, Node::getHalfNodeFromValue(moduleNodesCache[i]));
            modFileDeps[i / 2].isInterface = moduleNodesCache[i + 1].GetUint64();
        }

        for (uint64_t i = 0; i < headerUnitsNodesCache.Size(); ++i)
        {
            // If new, ensureSystemCheckCalled is called in SMFile::updateBTarget(1), else it is called in
            // SMFile::updateBTarget(2).
            hasManuallySpecifiedHeaderUnits = true;
            configuration->moduleFilesToTarget.emplace(Node::getHalfNodeFromValue(headerUnitsNodesCache[i]), this);
        }

        // Move to some other function, so it is not single-threaded.
        namespace CppBuild = Indices::BuildCache::CppBuild;
        Value &sourceValue = buildOrConfigCacheCopy[CppBuild::sourceFiles];
        sourceValue.Reserve(sourceValue.Size() + srcFileDeps.size(), cacheAlloc);

        Value &moduleValue = buildOrConfigCacheCopy[CppBuild::moduleFiles];
        moduleValue.Reserve(moduleValue.Size() + modFileDeps.size(), cacheAlloc);

        buildCacheFilesDirPathNode = Node::getHalfNodeFromValue(getConfigCache()[CppConfig::buildCacheFilesDirPath]);
        // Header-units size can't be known as these are dynamically discovered.
    }
}

void CppSourceTarget::getObjectFiles(vector<const ObjectFile *> *objectFiles, LOAT *loat) const
{
    btree_set<const SMFile *, IndexInTopologicalSortComparatorRoundZero> sortedSMFileDependencies;
    for (const SMFile &objectFile : modFileDeps)
    {
        sortedSMFileDependencies.emplace(&objectFile);
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
    for (CppSourceTarget *cppSourceTarget : reqDeps)
    {
        for (const InclNode &inclNode : cppSourceTarget->useReqIncls)
        {
            // Configure-time check.
            actuallyAddInclude(reqIncls, inclNode.node->filePath);
            reqIncls.emplace_back(inclNode);
        }
        reqCompilerFlags += cppSourceTarget->useReqCompilerFlags;
        for (const Define &define : cppSourceTarget->useReqCompileDefinitions)
        {
            reqCompileDefinitions.emplace(define);
        }
        reqCompilerFlags += cppSourceTarget->useReqCompilerFlags;

        for (InclNodeTargetMap &inclNodeTargetMap : cppSourceTarget->useReqHuDirs)
        {
            // Configure-time check
            actuallyAddInclude(reqHuDirs, this, inclNodeTargetMap.inclNode.node->filePath);
            reqHuDirs.emplace_back(inclNodeTargetMap);
        }

        if (!cppSourceTarget->useReqHuDirs.empty() || cppSourceTarget->hasManuallySpecifiedHeaderUnits)
        {
            cppSourceTarget->addDependencyDelayed<1>(*this);
        }
    }
}

void CppSourceTarget::adjustHeaderUnitsValueArrayPointers()
{
    // All header-units are found, so header-units value array size could be reserved
    // If a new header-unit was added in this run, sourceJson pointers will point to the newly allocated array if any

    namespace CppBuild = Indices::BuildCache::CppBuild;

    Value &headerUnitsValueArray = buildOrConfigCacheCopy[CppBuild::headerUnits];
    if (newHeaderUnitsSize)
    {
        for (uint64_t i = 0; i < newHeaderUnitsSize; ++i)
        {
            headerUnitsValueArray.PushBack(kArrayType, cacheAlloc);
        }
    }

    if (headerUnitScanned)
    {
        for (const SMFile *headerUnit : headerUnitsSet)
        {
            if (headerUnit->isSMRuleFileOutdated)
            {
                headerUnitsValueArray[headerUnit->indexInBuildCache].CopyFrom(headerUnit->sourceJson, cacheAlloc);
            }
        }
    }

    if (moduleFileScanned)
    {
        for (SMFile &smFile : modFileDeps)
        {
            if (smFile.isSMRuleFileOutdated)
            {
                buildOrConfigCacheCopy[CppBuild::moduleFiles][smFile.indexInBuildCache].CopyFrom(smFile.sourceJson,
                                                                                                 cacheAlloc);
            }
        }
    }
}

CppSourceTarget &CppSourceTarget::initializeUseReqInclsFromReqIncls()
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        for (const InclNode &include : reqIncls)
        {
            actuallyAddInclude(useReqIncls, include.node->filePath, include.isStandard, include.ignoreHeaderDeps);
        }
    }

    return *this;
}

CppSourceTarget &CppSourceTarget::initializePublicHuDirsFromReqIncls()
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        for (const InclNode &include : reqIncls)
        {
            if (evaluate(TreatModuleAsSource::NO))
            {
                actuallyAddInclude(reqHuDirs, this, include.node->filePath, include.isStandard,
                                   include.ignoreHeaderDeps);
                actuallyAddInclude(useReqHuDirs, this, include.node->filePath, include.isStandard,
                                   include.ignoreHeaderDeps);
            }
        }
    }

    return *this;
}

void CppSourceTarget::actuallyAddSourceFileConfigTime(const Node *node)
{
    // No check for uniques since this is checked in writeConfigCacheAtConfigTime
    buildOrConfigCacheCopy[Indices::ConfigCache::CppConfig::sourceFiles].PushBack(node->getValue(), cacheAlloc);
    if (Value &sourceBuildJson = buildCache[targetCacheIndex][Indices::BuildCache::CppBuild::sourceFiles];
        valueIndexInSubArray(sourceBuildJson, node->getValue()) == UINT64_MAX)
    {
        const uint64_t size = sourceBuildJson.Size();
        sourceBuildJson.PushBack(kArrayType, cacheAlloc);
        SourceNode::initializeSourceJson(sourceBuildJson[size], node, cacheAlloc, *this);
    }
}

void CppSourceTarget::actuallyAddModuleFileConfigTime(const Node *node, const bool isInterface)
{
    namespace CppConfig = Indices::ConfigCache::CppConfig;
    // No check for uniques since this is checked in writeConfigCacheAtConfigTime
    buildOrConfigCacheCopy[CppConfig::moduleFiles].PushBack(node->getValue(), cacheAlloc);
    buildOrConfigCacheCopy[CppConfig::moduleFiles].PushBack(static_cast<uint64_t>(isInterface), cacheAlloc);
    if (Value &moduleBuildJson = buildCache[targetCacheIndex][Indices::BuildCache::CppBuild::moduleFiles];
        valueIndexInSubArray(moduleBuildJson, node->getValue()) == UINT64_MAX)
    {
        const uint64_t size = moduleBuildJson.Size();
        moduleBuildJson.PushBack(kArrayType, cacheAlloc);
        SMFile::initializeModuleJson(moduleBuildJson[size], node, cacheAlloc, *this);
    }
}

void CppSourceTarget::actuallyAddHeaderUnitConfigTime(const Node *node)
{
    assert(bsMode == BSMode::CONFIGURE);
    namespace CppConfig = Indices::ConfigCache::CppConfig;
    // No check for uniques since this is checked in writeConfigCacheAtConfigTime
    buildOrConfigCacheCopy[CppConfig::headerUnits].PushBack(node->getValue(), cacheAlloc);
}

uint64_t CppSourceTarget::actuallyAddBigHuConfigTime(const Node *node, const string &headerUnit)
{
    namespace CppConfig = Indices::ConfigCache::CppConfig;
    // No check for uniques since this is checked in writeConfigCacheAtConfigTime
    buildOrConfigCacheCopy[CppConfig::headerUnits].PushBack(node->getValue(), cacheAlloc);
    Value &headerUnitJson = buildCache[targetCacheIndex][Indices::BuildCache::CppBuild::headerUnits];
    const uint64_t index = valueIndexInSubArray(headerUnitJson, node->getValue());
    if (index == UINT64_MAX)
    {
        const uint64_t size = headerUnitJson.Size();
        headerUnitJson.PushBack(kArrayType, cacheAlloc);
        SMFile::initializeModuleJson(headerUnitJson[size], node, cacheAlloc, *this);
        headerUnitJson[size][Indices::BuildCache::CppBuild::ModuleFiles::smRules].PushBack(
            Value().SetString(svtogsr(headerUnit), cacheAlloc), cacheAlloc);
        return size;
    }
    return index;
}

void CppSourceTarget::updateBTarget(Builder &builder, const unsigned short round)
{
    if (!round)
    {
        if (fileStatus)
        {
            // This is necessary since objectFile->outputFileNode is not updated once after it is compiled.
            assignFileStatusToDependents(0);
        }
    }
    else if (round == 1)
    {
        populateSourceNodes();
        adjustHeaderUnitsValueArrayPointers();

        if (headerUnitScanned || moduleFileScanned)
        {
            targetCacheDiskWriteManager.addNewBTargetInCopyJsonBTargetsCount(this);
        }
    }
    else if (round == 2)
    {
        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            writeTargetConfigCacheAtConfigureTime(true);
        }

        if constexpr (bsMode == BSMode::BUILD)
        {
            readConfigCacheAtBuildTime();
        }
        populateReqAndUseReqDeps();
        // Needed to maintain ordering between different includes specification.
        reqIncSizeBeforePopulate = reqIncls.size();
        populateTransitiveProperties();

        if constexpr (bsMode == BSMode::BUILD)
        {
            // getCompileCommand will be later on called concurrently therefore need to set this before.
            setCompileCommand();
            compileCommandWithTool.setCommand(configuration->compilerFeatures.compiler.bTPath.string() + " " +
                                              compileCommand);

            if (Value &headerUnitsValue = buildOrConfigCacheCopy[Indices::BuildCache::CppBuild::headerUnits];
                !headerUnitsValue.Empty())
            {
                oldHeaderUnits.reserve(headerUnitsValue.Size());
                for (uint64_t i = 0; i < headerUnitsValue.Size(); ++i)
                {
                    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;

                    string str;
                    str = vtosv(headerUnitsValue[i][ModuleFiles::smRules][ModuleFiles::SmRules::exportName]);
                    oldHeaderUnits.emplace_back(
                        this, Node::getHalfNodeFromValue(headerUnitsValue[i][ModuleFiles::fullPath]), str);
                    oldHeaderUnits[i].isAnOlderHeaderUnit = true;
                    oldHeaderUnits[i].indexInBuildCache = i;
                    headerUnitsSet.emplace(&oldHeaderUnits[i]);
                }

                lock_guard l(builder.executeMutex);
                for (SMFile *headerUnit : headerUnitsSet)
                {
                    builder.updateBTargetsIterator =
                        builder.updateBTargets.emplace(builder.updateBTargetsIterator, headerUnit);
                }
                builder.updateBTargetsSizeGoal += oldHeaderUnits.size();
                builder.cond.notify_one();
            }
        }

        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            writeTargetConfigCacheAtConfigureTime(false);
        }
        else
        {
            setSourceCompileCommandPrintFirstHalf();
            parseModuleSourceFiles(builder);
            populateResolveRequirePathDependencies();
        }
    }
}

void CppSourceTarget::copyJson()
{
    namespace CppBuild = Indices::BuildCache::CppBuild;
    Value &targetBuildCache = getBuildCache();
    if (headerUnitScanned)
    {
        // copy only header-units json. following does not need to be atomic since this is called single-threaded in
        // TargetCacheDiskWriteManager::endOfRound.
        targetBuildCache[CppBuild::headerUnits].CopyFrom(buildOrConfigCacheCopy[CppBuild::headerUnits], ralloc);
    }
    if (moduleFileScanned)
    {
        targetBuildCache[CppBuild::moduleFiles].CopyFrom(buildOrConfigCacheCopy[CppBuild::moduleFiles], ralloc);
    }
}

void CppSourceTarget::writeTargetConfigCacheAtConfigureTime(const bool before)
{
    if (before)
    {
        namespace ConfigCache = Indices::ConfigCache::CppConfig;

        writeIncDirsAtConfigTime(reqIncls, buildOrConfigCacheCopy[ConfigCache::reqInclsArray], cacheAlloc);
        writeIncDirsAtConfigTime(useReqIncls, buildOrConfigCacheCopy[ConfigCache::useReqInclsArray], cacheAlloc);
        writeIncDirsAtConfigTime(reqHuDirs, buildOrConfigCacheCopy[ConfigCache::reqHUDirsArray], cacheAlloc);
        writeIncDirsAtConfigTime(useReqHuDirs, buildOrConfigCacheCopy[ConfigCache::useReqHUDirsArray], cacheAlloc);

        testVectorHasUniqueElementsIncrement(buildOrConfigCacheCopy[ConfigCache::sourceFiles].GetArray(), "srcFileDeps",
                                             1);
        testVectorHasUniqueElementsIncrement(buildOrConfigCacheCopy[ConfigCache::moduleFiles].GetArray(), "modFileDeps",
                                             2);
        testVectorHasUniqueElements(reqIncls, "reqIncls");
        testVectorHasUniqueElements(useReqIncls, "useReqIncls");
        testVectorHasUniqueElements(reqHuDirs, "reqHuDirs");
        testVectorHasUniqueElements(useReqHuDirs, "useReqHuDirs");

        copyBackConfigCacheMutexLocked();
    }
}

void CppSourceTarget::readConfigCacheAtBuildTime()
{
    namespace ConfigCache = Indices::ConfigCache::CppConfig;

    // TODO
    // use-template

    Value &reqInclCache = getConfigCache()[ConfigCache::reqInclsArray];
    Value &useReqInclCache = getConfigCache()[ConfigCache::useReqInclsArray];
    Value &reqHUDirCache = getConfigCache()[ConfigCache::reqHUDirsArray];
    Value &useReqHUDirCache = getConfigCache()[ConfigCache::useReqHUDirsArray];

    constexpr uint8_t numOfElem = 3;
    reqIncls.reserve(reqInclCache.Size() / numOfElem);
    for (uint64_t i = 0; i < reqInclCache.Size(); i = i + numOfElem)
    {
        reqIncls.emplace_back(Node::getNodeFromValue(reqInclCache[i], false), reqInclCache[i + 1].GetUint64(),
                              reqInclCache[i + 2].GetUint64());
    }

    useReqIncls.reserve(useReqInclCache.Size() / numOfElem);
    for (uint64_t i = 0; i < useReqInclCache.Size(); i = i + numOfElem)
    {
        useReqIncls.emplace_back(Node::getNodeFromValue(useReqInclCache[i], false), useReqInclCache[i + 1].GetUint64(),
                                 useReqInclCache[i + 2].GetUint64());
    }

    constexpr uint8_t numOfHUDirElem = 5;
    reqHuDirs.reserve(reqHUDirCache.Size() / numOfHUDirElem);
    for (uint64_t i = 0; i < reqHUDirCache.Size(); i = i + numOfHUDirElem)
    {
        reqHuDirs.emplace_back(HeaderUnitNode(Node::getNodeFromValue(reqHUDirCache[i], false),
                                              reqHUDirCache[i + 3].GetUint64(), reqHUDirCache[i + 4].GetUint64(),
                                              reqHUDirCache[i + 1].GetUint64(), reqHUDirCache[i + 2].GetUint64()),
                               this);
    }

    useReqHuDirs.reserve(useReqHUDirCache.Size() / numOfHUDirElem);
    for (uint64_t i = 0; i < useReqHUDirCache.Size(); i = i + numOfHUDirElem)
    {
        useReqHuDirs.emplace_back(
            HeaderUnitNode(Node::getNodeFromValue(useReqHUDirCache[i], false), useReqHUDirCache[i + 3].GetUint64(),
                           useReqHUDirCache[i + 4].GetUint64(), useReqHUDirCache[i + 1].GetUint64(),
                           useReqHUDirCache[i + 2].GetUint64()),
            this);
    }
}

string CppSourceTarget::getTarjanNodeName() const
{
    return "CppSourceTarget " + configureNode->filePath + slashc + name;
}

CppSourceTarget &CppSourceTarget::publicCompilerFlags(const string &compilerFlags)
{
    reqCompilerFlags += compilerFlags;
    useReqCompilerFlags += compilerFlags;
    return *this;
}

CppSourceTarget &CppSourceTarget::privateCompilerFlags(const string &compilerFlags)
{
    reqCompilerFlags += compilerFlags;
    return *this;
}

CppSourceTarget &CppSourceTarget::interfaceCompilerFlags(const string &compilerFlags)
{
    useReqCompilerFlags += compilerFlags;
    return *this;
}

CppSourceTarget &CppSourceTarget::publicCompileDefinition(const string &cddName, const string &cddValue)
{
    reqCompileDefinitions.emplace(cddName, cddValue);
    useReqCompileDefinitions.emplace(cddName, cddValue);
    return *this;
}

CppSourceTarget &CppSourceTarget::privateCompileDefinition(const string &cddName, const string &cddValue)
{
    reqCompileDefinitions.emplace(cddName, cddValue);
    return *this;
}

CppSourceTarget &CppSourceTarget::interfaceCompileDefinition(const string &cddName, const string &cddValue)
{
    useReqCompileDefinitions.emplace(cddName, cddValue);
    return *this;
}

void CppSourceTarget::parseRegexSourceDirs(bool assignToSourceNodes, const string &sourceDirectory, string regex,
                                           const bool recursive)
{
    if (evaluate(TreatModuleAsSource::YES))
    {
        assignToSourceNodes = true;
    }

    if constexpr (bsMode == BSMode::BUILD)
    {
        // Initialized in CppSourceTarget round 2
        return;
    }

    const SourceDirectory dir{sourceDirectory, std::move(regex), recursive};
    auto addNewFile = [&](const auto &k) {
        if (k.is_regular_file() && regex_match(k.path().filename().string(), std::regex(dir.regex)))
        {
            Node *node = Node::getNodeFromNonNormalizedPath(k.path(), true);
            if (assignToSourceNodes)
            {
                actuallyAddSourceFileConfigTime(node);
            }
            else
            {
                actuallyAddModuleFileConfigTime(node, false);
            }
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

    const CompilerFlags &flags = configuration->compilerFlags;
    const Compiler &compiler = configuration->compilerFeatures.compiler;

    if (compiler.bTFamily == BTFamily::GCC)
    {
        // string str = cSourceTarget == CSourceTargetEnum::YES ? "-xc" : "-xc++";
        compileCommand +=
            flags.LANG + flags.OPTIONS + flags.OPTIONS_COMPILE + flags.OPTIONS_COMPILE_CPP + flags.DEFINES_COMPILE_CPP;
    }
    else if (compiler.bTFamily == BTFamily::MSVC)
    {
        // TODO
        // Remember this for CSourceTarget
        // const string str = cSourceTarget == CSourceTargetEnum::YES ? "-TC" : "-TP";
        const string str = "-TP";
        compileCommand += str + " " + flags.CPP_FLAGS_COMPILE_CPP + flags.CPP_FLAGS_COMPILE + flags.OPTIONS_COMPILE +
                          flags.OPTIONS_COMPILE_CPP;
    }

    if (compiler.bTFamily == BTFamily::MSVC && evaluate(TranslateInclude::YES))
    {
        compileCommand += "/translateInclude ";
    }

    auto getIncludeFlag = [&compiler] {
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            return "/I ";
        }
        return "-I ";
    };

    compileCommand += reqCompilerFlags;

    for (const auto &i : reqCompileDefinitions)
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

    // Following set is needed because otherwise InclNode propogated from other reqDeps won't have ordering,
    // because reqDeps in DS is set. Because of weak ordering this will hurt the caching. Now,
    // reqIncls can be made set, but this is not done to maintain specification order for include-dirs

    // I think ideally this should not be support this. A same header-file should not present in more than one
    // header-file.

    auto it = reqIncls.begin();
    for (unsigned short i = 0; i < reqIncSizeBeforePopulate; ++i)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(it->node->filePath) + " ");
        ++it;
    }

    btree_set<string> includes;

    for (; it != reqIncls.end(); ++it)
    {
        includes.emplace(it->node->filePath);
    }

    for (const string &include : includes)
    {
        compileCommand.append(getIncludeFlag() + addQuotes(include) + " ");
    }
}

void CppSourceTarget::setSourceCompileCommandPrintFirstHalf()
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;
    const Compiler &compiler = configuration->compilerFeatures.compiler;

    const path p;
    if (ccpSettings.tool.printLevel != PathPrintLevel::NO)
    {
        sourceCompileCommandPrintFirstHalf +=
            getReducedPath(compiler.bTPath.lexically_normal().string(), ccpSettings.tool) + " ";
    }

    if (ccpSettings.infrastructureFlags)
    {
        const CompilerFlags &flags = configuration->compilerFlags;
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
        sourceCompileCommandPrintFirstHalf += reqCompilerFlags;
    }

    for (const auto &i : reqCompileDefinitions)
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

    auto getIncludeFlag = [&compiler] {
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
            if (ccpSettings.standardIncludeDirs.printLevel != PathPrintLevel::NO)
            {
                sourceCompileCommandPrintFirstHalf +=
                    getIncludeFlag() + getReducedPath(include.node->filePath, ccpSettings.standardIncludeDirs) + " ";
            }
        }
        else
        {
            if (ccpSettings.includeDirs.printLevel != PathPrintLevel::NO)
            {
                sourceCompileCommandPrintFirstHalf +=
                    getIncludeFlag() + getReducedPath(include.node->filePath, ccpSettings.includeDirs) + " ";
            }
        }
    }
}

string &CppSourceTarget::getSourceCompileCommandPrintFirstHalf()
{
    if (sourceCompileCommandPrintFirstHalf.empty())
    {
        setSourceCompileCommandPrintFirstHalf();
    }
    return sourceCompileCommandPrintFirstHalf;
}

string CppSourceTarget::getDependenciesPString() const
{
    string deps;
    for (const CppSourceTarget *cppSourceTarget : reqDeps)
    {
        deps += cppSourceTarget->name + '\n';
    }
    return deps;
}

void CppSourceTarget::resolveRequirePaths()
{
    for (SMFile &smFile : modFileDeps)
    {
        namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;

        for (Value &require : smFile.sourceJson[ModuleFiles::smRules][ModuleFiles::SmRules::moduleArray].GetArray())
        {
            namespace SingleModuleDep = ModuleFiles::SmRules::SingleModuleDep;

            if (require[SingleModuleDep::logicalName] == Value(svtogsr(smFile.logicalName)))
            {
                printErrorMessage(FORMAT("In target\n{}\nModule\n{}\n can not depend on itself.\n", getTarjanNodeName(),
                                         smFile.node->filePath));
            }

            const SMFile *found = nullptr;

            // Even if found, we continue the search to ensure that no two files are providing the same module in
            // the module-files of the target and its dependencies
            RequireNameTargetId req(id, string(require[SingleModuleDep::logicalName].GetString(),
                                               require[SingleModuleDep::logicalName].GetStringLength()));

            const bool isInterface = req.requireName.contains(':');

            using Map = decltype(requirePaths2);
            if (requirePaths2.if_contains(
                    req, [&](const Map::value_type &value) { found = const_cast<SMFile *>(value.second); }))
            {
            }

            // An interface module is searched only in the module files of the current target.
            if (!isInterface)
            {
                const SMFile *found2 = nullptr;
                for (CppSourceTarget *cppSourceTarget : reqDeps)
                {
                    req.id = cppSourceTarget->id;

                    if (requirePaths2.if_contains(
                            req, [&](const Map::value_type &value) { found2 = const_cast<SMFile *>(value.second); }))
                    {
                        if (found)
                        {
                            // Module was already found so error-out
                            printErrorMessage(
                                FORMAT("Module name:\n {}\n Is Being Provided By 2 different files:\n1){}\n"
                                       "from target\n{}\n2){}\n from target\n{}\n",
                                       getTarjanNodeName(), getDependenciesPString(),
                                       string(require[SingleModuleDep::logicalName].GetString(),
                                              require[SingleModuleDep::logicalName].GetStringLength()),
                                       found->node->filePath, found->node->filePath));
                        }
                        found = found2;
                    }
                }
            }

            if (found)
            {
                smFile.addDependencyDelayed<0>(const_cast<SMFile &>(*found));
                if (!atomic_ref(smFile.fileStatus).load())
                {
                    if (require[SingleModuleDep::fullPath] != found->objectFileOutputFileNode->getValue())
                    {
                        atomic_ref(smFile.fileStatus).store(true);
                    }
                }
                smFile.valueObjectFileMapping.emplace_back(&require, found->objectFileOutputFileNode);
            }
            else
            {
                if (isInterface)
                {
                    printErrorMessage(FORMAT("No File in the target\n{}\n provides this module\n{}.\n",
                                             getTarjanNodeName(),
                                             string(require[SingleModuleDep::logicalName].GetString(),
                                                    require[SingleModuleDep::logicalName].GetStringLength())));
                }
                else
                {
                    printErrorMessage(
                        FORMAT("No File in the target\n{}\n or in its dependencies\n{}\n provides this module\n{}.\n",
                               getTarjanNodeName(), getDependenciesPString(),
                               string(require[SingleModuleDep::logicalName].GetString(),
                                      require[SingleModuleDep::logicalName].GetStringLength())));
                }
            }
        }
    }
}

void CppSourceTarget::populateSourceNodes()
{
    Value &sourceFilesJson = buildOrConfigCacheCopy[Indices::BuildCache::CppBuild::sourceFiles];

    for (const SourceNode &sourceNodeConst : srcFileDeps)
    {
        auto &sourceNode = const_cast<SourceNode &>(sourceNodeConst);

        addDependencyDelayed<0>(sourceNode);

        if (const size_t fileIt = valueIndexInSubArray(sourceFilesJson, Value(sourceNode.node->getValue()));
            fileIt != UINT64_MAX)
        {
            sourceNode.sourceJson.CopyFrom(sourceFilesJson[fileIt], cacheAlloc);
            sourceNode.indexInBuildCache = fileIt;
        }
        else
        {
            // TODO
            // If Mini-Target, then initialize the json else error out. Should not happen for full-targets.
        }
    }
}

void CppSourceTarget::parseModuleSourceFiles(Builder &)
{
    Value &moduleFilesJson = buildOrConfigCacheCopy[Indices::BuildCache::CppBuild::moduleFiles];

    for (const SMFile &smFileConst : modFileDeps)
    {
        auto &smFile = const_cast<SMFile &>(smFileConst);

        addDependencyDelayed<0>(smFile);
        resolveRequirePathBTarget.addDependencyDelayed<1>(smFile);
        addDependencyDelayed<1>(smFile);

        if (const size_t fileIt = valueIndexInSubArray(moduleFilesJson, Value(smFile.node->getValue()));
            fileIt != UINT64_MAX)
        {
            smFile.sourceJson.CopyFrom(moduleFilesJson[fileIt], cacheAlloc);
            smFile.indexInBuildCache = fileIt;
        }
        else
        {
            // TODO
            // If Mini-Target, then initialize the json else error out. Should not happen for full-targets.
        }
    }
}

void CppSourceTarget::populateResolveRequirePathDependencies()
{
    for (CppSourceTarget *cppSourceTarget : reqDeps)
    {
        if (!cppSourceTarget->modFileDeps.empty())
        {
            resolveRequirePathBTarget.addDependency<1>(cppSourceTarget->resolveRequirePathBTarget);
        }
    }
}

string CppSourceTarget::getInfrastructureFlags(const Compiler &compiler, const bool showIncludes)
{
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        string str = "-c /nologo ";
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
        return "-c -MMD";
    }
    return "";
}

string CppSourceTarget::getCompileCommandPrintSecondPart(const SourceNode &sourceNode) const
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;

    const Compiler &compiler = configuration->compilerFeatures.compiler;
    string command;
    if (ccpSettings.infrastructureFlags)
    {
        command += getInfrastructureFlags(compiler, true) + " ";
    }
    if (ccpSettings.sourceFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(sourceNode.node->filePath, ccpSettings.sourceFile) + " ";
    }
    if (ccpSettings.infrastructureFlags)
    {
        command += compiler.bTFamily == BTFamily::MSVC ? "/Fo" : "-o ";
    }
    if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(sourceNode.objectFileOutputFileNode->filePath, ccpSettings.objectFile) + " ";
    }
    return command;
}

string CppSourceTarget::getCompileCommandPrintSecondPartSMRule(const SMFile &smFile) const
{
    const CompileCommandPrintSettings &ccpSettings = settings.ccpSettings;

    const Compiler &compiler = configuration->compilerFeatures.compiler;
    string command;

    if (ccpSettings.sourceFile.printLevel != PathPrintLevel::NO)
    {
        command += getReducedPath(smFile.node->filePath, ccpSettings.sourceFile) + " ";
    }
    if (ccpSettings.infrastructureFlags)
    {
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            const string translateIncludeFlag = GET_FLAG_evaluate(TranslateInclude::YES, "/translateInclude ");
            command += translateIncludeFlag + " /nologo /showIncludes /scanDependencies ";
        }
    }
    if (ccpSettings.objectFile.printLevel != PathPrintLevel::NO)
    {
        command +=
            getReducedPath(buildCacheFilesDirPathNode->filePath + slashc + smFile.node->getFileName() + ".smrules",
                           ccpSettings.objectFile) +
            " ";
    }

    return command;
}

PostCompile CppSourceTarget::CompileSMFile(const SMFile &smFile)
{
    string finalCompileCommand = compileCommand;

    const Compiler &compiler = configuration->compilerFeatures.compiler;
    for (const SMFile *smFileLocal : smFile.allSMFileDependenciesRoundZero)
    {
        finalCompileCommand += smFileLocal->getRequireFlag(smFile);
    }
    finalCompileCommand += getInfrastructureFlags(compiler, false) + " " + addQuotes(smFile.node->filePath) + " ";

    finalCompileCommand += smFile.getFlag();

    return PostCompile{
        *this,
        compiler.bTPath,
        finalCompileCommand,
        getSourceCompileCommandPrintFirstHalf() + smFile.getModuleCompileCommandPrintLastHalf(),
    };
}

PostCompile CppSourceTarget::updateSourceNodeBTarget(const SourceNode &sourceNode)
{
    const string compileFileName = sourceNode.node->getFileName();

    string finalCompileCommand = compileCommand + " ";

    const Compiler &compiler = configuration->compilerFeatures.compiler;

    finalCompileCommand += getInfrastructureFlags(compiler, true) + " " + addQuotes(sourceNode.node->filePath) + " ";
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        finalCompileCommand +=
            "/Fo" + addQuotes(buildCacheFilesDirPathNode->filePath + slashc + compileFileName + ".o");
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
        finalCompileCommand +=
            "-o " + addQuotes(buildCacheFilesDirPathNode->filePath + slashc + compileFileName + ".o");
    }

    return PostCompile{
        *this,
        compiler.bTPath,
        finalCompileCommand,
        getSourceCompileCommandPrintFirstHalf() + getCompileCommandPrintSecondPart(sourceNode),
    };
}

PostCompile CppSourceTarget::GenerateSMRulesFile(const SMFile &smFile, const bool printOnlyOnError)
{
    string finalCompileCommand = compileCommand + addQuotes(smFile.node->filePath) + " ";

    const Compiler &compiler = configuration->compilerFeatures.compiler;
    const ScannerTool &scanner = configuration->compilerFeatures.scanner;
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        // If JSon is already set, then this should not be called.
        if (smFile.isSMRulesJsonSet)
        {
            HMAKE_HMAKE_INTERNAL_ERROR
            exit(EXIT_FAILURE);
        }
        finalCompileCommand +=
            "/nologo /showIncludes /scanDependencies " +
            addQuotes(buildCacheFilesDirPathNode->filePath + slashc + smFile.getOutputFileName() + ".smrules") + " ";

        return printOnlyOnError ? PostCompile(*this, compiler.bTPath, finalCompileCommand, "")
                                : PostCompile(*this, compiler.bTPath, finalCompileCommand,
                                              getSourceCompileCommandPrintFirstHalf() +
                                                  getCompileCommandPrintSecondPartSMRule(smFile));
    }
    if (compiler.bTFamily == BTFamily::GCC)
    {
        // clang flags. gcc not yet supported.
        finalCompileCommand =
            "-format=p1689 -- " + compiler.bTPath.string() + " " + finalCompileCommand + " -c -MMD -MF ";
        finalCompileCommand +=
            addQuotes(buildCacheFilesDirPathNode->filePath + slashc + smFile.node->getFileName() + ".d");

        return printOnlyOnError ? PostCompile(*this, scanner.bTPath, finalCompileCommand, "")
                                : PostCompile(*this, scanner.bTPath, finalCompileCommand,
                                              getSourceCompileCommandPrintFirstHalf() +
                                                  getCompileCommandPrintSecondPartSMRule(smFile));
    }
    printErrorMessage("Generate SMRules not supported for this compiler\n");
}

bool operator<(const CppSourceTarget &lhs, const CppSourceTarget &rhs)
{
    return lhs.name < rhs.name;
}

template <> DSC<CppSourceTarget>::DSC(CppSourceTarget *ptr, PLOAT *ploat_, const bool defines, string define_)
{
    objectFileProducer = ptr;
    ploat = ploat_;
    if (ploat_)
    {
        ploat->objectFileProducers.emplace(objectFileProducer);

        if (define_.empty())
        {
            define = ploat->getOutputName();
            transform(define.begin(), define.end(), define.begin(), toupper);
            define += "_EXPORT";
        }
        else
        {
            define = std::move(define_);
        }

        if (defines)
        {
            defineDllPrivate = DefineDLLPrivate::YES;
            defineDllInterface = DefineDLLInterface::YES;
        }

        if (defineDllPrivate == DefineDLLPrivate::YES)
        {
            if (ploat->evaluate(TargetType::LIBRARY_SHARED))
            {
                if (ptr->configuration->compilerFeatures.compiler.bTFamily == BTFamily::MSVC)
                {
                    ptr->reqCompileDefinitions.emplace(Define(define, "__declspec(dllexport)"));
                }
                else
                {
                    ptr->reqCompileDefinitions.emplace(
                        Define(define, "\"__attribute__ ((visibility (\\\"default\\\")))\""));
                }
            }
            else
            {
                ptr->reqCompileDefinitions.emplace(Define(define, ""));
            }
        }
    }
}

template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::save(CppSourceTarget &ptr)
{
    if (!stored)
    {
        stored = static_cast<CppSourceTarget *>(objectFileProducer);
    }
    objectFileProducer = &ptr;
    return *this;
}

template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::saveAndReplace(CppSourceTarget &ptr)
{
    save(ptr);

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        namespace CppConfig = Indices::ConfigCache::CppConfig;
        const Value &modulesConfigCache = stored->buildOrConfigCacheCopy[CppConfig::moduleFiles];
        for (uint64_t i = 0; i < modulesConfigCache.Size(); i = i + 2)
        {
            if (modulesConfigCache[i + 1].GetBool())
            {
                ptr.moduleFiles(Node::getNodeFromValue(modulesConfigCache[i], true)->filePath);
            }
        }
    }

    for (auto &[inclNode, cppSourceTarget] : stored->reqHuDirs)
    {
        actuallyAddInclude(ptr.reqHuDirs, &ptr, inclNode.node->filePath, inclNode.isStandard,
                           inclNode.ignoreHeaderDeps);
    }
    for (auto &[inclNode, cppSourceTarget] : stored->useReqHuDirs)
    {
        actuallyAddInclude(ptr.useReqHuDirs, &ptr, inclNode.node->filePath, inclNode.isStandard,
                           inclNode.ignoreHeaderDeps);
    }
    ptr.reqCompileDefinitions = stored->reqCompileDefinitions;
    ptr.reqIncls = stored->reqIncls;

    ptr.useReqCompileDefinitions = stored->useReqCompileDefinitions;
    ptr.useReqIncls = stored->useReqIncls;
    return *this;
}

template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::restore()
{
    objectFileProducer = stored;
    return *this;
}
