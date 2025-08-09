
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

RequireNameTargetId::RequireNameTargetId(const uint64_t id_, string_view requireName_)
    : id(id_), requireName(requireName_)
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
        if (buildCacheFilesDirPath.empty())
        {
            buildCacheFilesDirPath = configureNode->filePath + slashc + name;
        }
        create_directories(buildCacheFilesDirPath);
        buildCacheFilesDirPathNode = Node::addHalfNodeFromNormalizedStringSingleThreaded(buildCacheFilesDirPath);
    }

    if constexpr (bsMode == BSMode::BUILD)
    {
        readConfigCacheAtBuildTime();
    }
}

void CppSourceTarget::getObjectFiles(vector<const ObjectFile *> *objectFiles, LOAT *loat) const
{
    btree_set<const SMFile *, IndexInTopologicalSortComparatorRoundZero> sortedSMFileDependencies;
    for (const SMFile *objectFile : modFileDeps)
    {
        sortedSMFileDependencies.emplace(objectFile);
    }

    for (const SMFile *objectFile : sortedSMFileDependencies)
    {
        objectFiles->emplace_back(objectFile);
    }

    for (const SourceNode *objectFile : srcFileDeps)
    {
        objectFiles->emplace_back(objectFile);
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

void CppSourceTarget::actuallyAddSourceFileConfigTime(Node *node)
{
    for (const SourceNode *source : srcFileDeps)
    {
        if (source->node == node)
        {
        }
    }
    srcFileDeps.emplace_back(this, node);
}

void CppSourceTarget::actuallyAddModuleFileConfigTime(Node *node, const bool isInterface)
{
    for (const SMFile *smFile : modFileDeps)
    {
        if (smFile->node == node)
        {
        }
    }
    const auto &m = modFileDeps.emplace_back(this, node);
    m->isInterface = isInterface;
}

void CppSourceTarget::actuallyAddHeaderUnitConfigTime(const Node *node)
{
    // TODO
    /*assert(bsMode == BSMode::CONFIGURE);
    namespace CppConfig = Indices::ConfigCache::CppConfig;
    // No check for uniques since this is checked in writeConfigCacheAtConfigTime
    buildOrConfigCacheCopy[CppConfig::headerUnits].PushBack(node->getValue(), cacheAlloc);*/
}

/*
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
}*/

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
        initializeCppBuildCache();
        if (headerUnitScanned || moduleFileScanned)
        {
            targetCacheDiskWriteManager.updateCacheOnRoundEndCppSourceTarget(this);
        }
    }
    else if (round == 2)
    {
        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            writeCacheAtConfigTime(true);
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

            cppBuildCache.initialize(targetCacheIndex);
            if (!cppBuildCache.headerUnits.empty())
            {
                oldHeaderUnits.reserve(cppBuildCache.headerUnits.size());
                for (uint64_t i = 0; i < cppBuildCache.headerUnits.size(); ++i)
                {
                    namespace ModuleFiles = Indices::BuildCache::CppBuild::ModuleFiles;

                    oldHeaderUnits.emplace_back(this, cppBuildCache.headerUnits[i].srcFile.fullPath);
                    oldHeaderUnits[i].isAnOlderHeaderUnit = true;
                    oldHeaderUnits[i].indexInBuildCache = i;
                    headerUnitsSet.emplace(&oldHeaderUnits[i]);
                }

                lock_guard l(builder.executeMutex);
                for (SMFile *headerUnit : headerUnitsSet)
                {
                    builder.updateBTargets.emplace(headerUnit);
                }
                builder.updateBTargetsSizeGoal += oldHeaderUnits.size();
                builder.cond.notify_one();
            }
        }

        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            writeCacheAtConfigTime(false);
        }
        else
        {
            setSourceCompileCommandPrintFirstHalf();
            populateResolveRequirePathDependencies();
        }
    }
}

void CppSourceTarget::copyBuildCache(vector<char> &buildBuffer)
{
    for (const SourceNode *source : srcFileDeps)
    {
        buildBuffer.insert(buildBuffer.end(), source->buildCache, source->buildCache + source->buildCacheSize);
    }

    for (const SMFile *smFile : modFileDeps)
    {
        buildBuffer.insert(buildBuffer.end(), smFile->buildCache, smFile->buildCache + smFile->buildCacheSize);
    }

    for (const SMFile *smFile : headerUnitsSet)
    {
        buildBuffer.insert(buildBuffer.end(), smFile->buildCache, smFile->buildCache + smFile->buildCacheSize);
    }
}

void CppSourceTarget::checkAndCopyBuildCache(vector<char> &buildBuffer)
{
    if (newHeaderUnitsSize)
    {
        auto *headerUnitsCache = new vector<BuildCache::Cpp::ModuleFile>{};
        headerUnitsCache->reserve(newHeaderUnitsSize + oldHeaderUnits.size());
        headerUnitsCache->insert(headerUnitsCache->end(), cppBuildCache.headerUnits.begin(),
                                 cppBuildCache.headerUnits.end());
    }

    copyBuildCache(buildBuffer);
}

template <typename T> uint32_t findNodeInSourceCache(const span<T> sourceCache, const Node *node)
{
    for (uint32_t i = 0; i < sourceCache.size(); ++i)
    {
        if (sourceCache[i].fullPath == node)
        {
            return i;
        }
    }
    return -1;
}

template <typename T, typename U> void adjustBuildCache(span<T> &oldCache, const vector<U *> &sourceFiles)
{
    auto *newCache = new vector<T>{oldCache.begin(), oldCache.end()};
    newCache->resize(oldCache.size() + sourceFiles.size());

    uint32_t newlyFounded = 0;
    for (uint32_t i = 0; i < sourceFiles.size(); ++i)
    {
        if (const uint32_t cacheIndex = findNodeInSourceCache(oldCache, sourceFiles[i]->node); cacheIndex == -1)
        {
            std::swap((*newCache)[i], (*newCache)[newlyFounded + oldCache.size()]);
            ++newlyFounded;
        }
        else
        {
            std::swap((*newCache)[i], (*newCache)[cacheIndex]);
        }
    }

    newCache->resize(newlyFounded + oldCache.size());
    oldCache = span(newCache->data(), newlyFounded + oldCache.size());
}

void CppSourceTarget::writeCacheAtConfigTime(const bool before)
{
    if (before)
    {
        auto *buffer = new vector<char>;
        writeIncDirsAtConfigTime(buffer, reqIncls);
        writeIncDirsAtConfigTime(buffer, useReqIncls);
        writeIncDirsAtConfigTime(buffer, reqHuDirs);
        writeIncDirsAtConfigTime(buffer, useReqHuDirs);

        for (const SourceNode *source : srcFileDeps)
        {
            writeNode(*buffer, source->node);
        }

        for (const SMFile *smFile : modFileDeps)
        {
            writeNode(*buffer, smFile->node);
            writeBool(*buffer, smFile->isInterface);
        }

        configCacheTargets[targetCacheIndex].configCache = span{buffer->data(), buffer->size()};

        cppBuildCache.initialize(targetCacheIndex);
        adjustBuildCache(cppBuildCache.srcFiles, srcFileDeps);
        adjustBuildCache(cppBuildCache.modFiles, modFileDeps);
    }
}

void CppSourceTarget::readConfigCacheAtBuildTime()
{
    namespace ConfigCache = Indices::ConfigCache::CppConfig;

    const span<char> configCache = configCacheTargets[targetCacheIndex].configCache;

    uint32_t configRead = 0;
    const char *ptr = configCache.data();

    readInclDirsAtBuildTime(ptr + configRead, configRead, reqIncls);
    readInclDirsAtBuildTime(ptr + configRead, configRead, useReqIncls);
    readInclDirsAtBuildTime(ptr + configRead, configRead, reqHuDirs);
    readInclDirsAtBuildTime(ptr + configRead, configRead, useReqHuDirs);

    const uint32_t sourceSize = readUint32(ptr + configRead, configRead);
    for (uint32_t i = 0; i < sourceSize; ++i)
    {
        SourceNode *srcNode = srcFileDeps.emplace_back(new SourceNode(this, readNode(ptr + configRead, configRead)));
        addDependencyNoMutex<0>(*srcNode);
    }

    const uint32_t modSize = readUint32(ptr + configRead, configRead);
    for (uint32_t i = 0; i < modSize; ++i)
    {
        SMFile *smFile = modFileDeps.emplace_back(new SMFile(this, readNode(ptr + configRead, configRead)));
        smFile->isInterface = readBool(ptr + configRead, configRead);
        addDependencyNoMutex<0>(*smFile);
        resolveRequirePathBTarget.addDependencyNoMutex<0>(*smFile);
        addDependencyNoMutex<1>(*smFile);
    }

    if (configRead != configCache.size())
    {
        // error
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
    for (uint32_t i = 0; i < modFileDeps.size(); ++i)
    {
        SMFile *smFile = modFileDeps[i];

        for (BuildCache::Cpp::ModuleFile::SmRules::SingleModuleDep &require :
             cppBuildCache.modFiles[i].smRules.moduleArray)
        {
            if (require.logicalName == smFile->logicalName)
            {
                printErrorMessage(FORMAT("In target\n{}\nModule\n{}\n can not depend on itself.\n", getTarjanNodeName(),
                                         smFile->node->filePath));
            }

            const SMFile *found = nullptr;

            // Even if found, we continue the search to ensure that no two files are providing the same module in
            // the module-files of the target and its dependencies
            RequireNameTargetId req(id, require.logicalName);

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
                                       getTarjanNodeName(), getDependenciesPString(), require.logicalName,
                                       found->node->filePath, found->node->filePath));
                        }
                        found = found2;
                    }
                }
            }

            if (found)
            {
                smFile->addDependencyDelayed<0>(const_cast<SMFile &>(*found));
                if (!smFile->fileStatus && !atomic_ref(smFile->fileStatus).load())
                {
                    if (require.fullPath != found->objectFileOutputFileNode)
                    {
                        atomic_ref(smFile->fileStatus).store(true);
                    }
                }
                smFile->logicalNameObjectFileMapping.emplace_back(require.logicalName, found->objectFileOutputFileNode);
            }
            else
            {
                if (isInterface)
                {
                    printErrorMessage(FORMAT("No File in the target\n{}\n provides this module\n{}.\n",
                                             getTarjanNodeName(), require.logicalName));
                }
                else
                {
                    printErrorMessage(
                        FORMAT("No File in the target\n{}\n or in its dependencies\n{}\n provides this module\n{}.\n",
                               getTarjanNodeName(), getDependenciesPString(), require.logicalName));
                }
            }
        }
    }
}

void CppSourceTarget::initializeCppBuildCache() const
{
    for (uint32_t i = 0; i < srcFileDeps.size(); ++i)
    {
        srcFileDeps[i]->indexInBuildCache = i;
    }

    for (uint32_t i = 0; i < srcFileDeps.size(); ++i)
    {
        modFileDeps[i]->indexInBuildCache = i;
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
            errorExit();
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
    /*save(ptr);

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
    return *this*/;
}

template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::restore()
{
    objectFileProducer = stored;
    return *this;
}
