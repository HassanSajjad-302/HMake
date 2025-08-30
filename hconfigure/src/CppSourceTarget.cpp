
#ifdef USE_HEADER_UNITS
import "CppSourceTarget.hpp";
import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "ConfigHelpers.hpp";
import "Configuration.hpp";
import "LOAT.hpp";
import "rapidhash.h";
import "CacheWriteManager.hpp";
import "Utilities.hpp";
import <filesystem>;
import <fstream>;
import <regex>;
import <utility>;
#else
#include "CppSourceTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "CacheWriteManager.hpp"
#include "ConfigHelpers.hpp"
#include "Configuration.hpp"
#include "LOAT.hpp"
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
    : ObjectFileProducerWithDS(name_, false, false), TargetCache(name), configuration(configuration_)
{
    initializeCppSourceTarget(name_, "");
}

CppSourceTarget::CppSourceTarget(const bool buildExplicit, const string &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, buildExplicit, false), TargetCache(name), configuration(configuration_)
{
    initializeCppSourceTarget(name_, "");
}

CppSourceTarget::CppSourceTarget(string buildCacheFilesDirPath_, const string &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, false, false), TargetCache(name), configuration(configuration_)
{
    initializeCppSourceTarget(name_, configureNode->filePath + slashc + std::move(buildCacheFilesDirPath_));
}

CppSourceTarget::CppSourceTarget(string buildCacheFilesDirPath_, const bool buildExplicit, const string &name_,
                                 Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, buildExplicit, false), TargetCache(name), configuration(configuration_)
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
        myBuildDir = Node::addHalfNodeFromNormalizedStringSingleThreaded(buildCacheFilesDirPath);
    }

    if constexpr (bsMode == BSMode::BUILD)
    {
        cppSourceTargets[cacheIndex] = this;
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

void CppSourceTarget::updateBuildCache(void *ptr)
{
    static_cast<SourceNode *>(ptr)->updateBuildCache();
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

        for (HuTargetPlusDir &inclNodeTargetMap : cppSourceTarget->useReqHuDirs)
        {
            // Configure-time check
            actuallyAddInclude(reqHuDirs, this, inclNodeTargetMap.inclNode.node->filePath);
            reqHuDirs.emplace_back(inclNodeTargetMap);
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
            printErrorMessage(
                FORMAT("Attempting to add {} twice in source-files in cpptarget {}. second insertiion ignored.\n",
                       node->filePath, name));
            return;
        }
    }
    srcFileDeps.emplace_back(new SourceNode(this, node));
}

void CppSourceTarget::actuallyAddModuleFileConfigTime(Node *node, const bool isInterface)
{
    for (const SMFile *smFile : modFileDeps)
    {
        if (smFile->node == node)
        {
            printErrorMessage(
                FORMAT("Attempting to add {} twice in module-files in cpptarget {}. second insertiion ignored.\n",
                       node->filePath, name));
            return;
        }
    }
    SMFile *smFile = modFileDeps.emplace_back(new SMFile(this, node));
    smFile->isInterface = isInterface;
    string fileName = smFile->node->getFileName();
    string ext = {fileName.begin() + fileName.find_last_of('.') + 1, fileName.end()};
    if (ext == ".cppm" || ext == ".ixx")
    {
        smFile->type = fileName.contains('-') ? SM_FILE_TYPE::PRIMARY_EXPORT : SM_FILE_TYPE::PARTITION_EXPORT;
    }
    else
    {
        smFile->type = SM_FILE_TYPE::PARTITION_IMPLEMENTATION;
    }
}

void CppSourceTarget::actuallyAddHeaderUnitConfigTime(Node *node)
{
    for (const SMFile &smFile : oldHeaderUnits)
    {
        if (smFile.node == node)
        {
            printErrorMessage(
                FORMAT("Attempting to add {} twice in header-units in cpptarget {}. second insertiion ignored.\n",
                       node->filePath, name));
            return;
        }
    }
    oldHeaderUnits.emplace_back(this, node);
}

uint64_t CppSourceTarget::actuallyAddBigHuConfigTime(const Node *node, const string &headerUnit)
{
    HMAKE_HMAKE_INTERNAL_ERROR
    return 33;
    /*namespace CppConfig = Indices::ConfigCache::CppConfig;
    // No check for uniques since this is checked in writeConfigCacheAtConfigTime
    buildOrConfigCacheCopy[CppConfig::headerUnits].PushBack(node->getValue(), cacheAlloc);
    Value &headerUnitJson = buildCacheBuffer[targetCacheIndex][Indices::BuildCache::CppBuild::headerUnits];
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
    return index;*/
}

void CppSourceTarget::updateBTarget(Builder &builder, const unsigned short round, bool &isComplete)
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
        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            writeCacheAtConfigTime(true);
        }

        populateReqAndUseReqDeps();
        // Needed to maintain ordering between different includes specification.
        reqIncSizeBeforePopulate = reqIncls.size();
        populateTransitiveProperties();

        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            return;
        }

        // getCompileCommand will be later on called concurrently therefore need to set this before.
        setCompileCommand();
        compileCommandWithTool.setCommand(configuration->compilerFeatures.compiler.bTPath.string() + " " +
                                          compileCommand);

        cppBuildCache.deserialize(cacheIndex);
        for (uint32_t i = 0; i < srcFileDeps.size(); ++i)
        {
            srcFileDeps[i]->initializeBuildCache(i);
        }
        for (uint32_t i = 0; i < modFileDeps.size(); ++i)
        {
            modFileDeps[i]->initializeBuildCache(i);
        }
        setSourceCompileCommandPrintFirstHalf();
        for (const CppSourceTarget *cppSourceTarget : reqDeps)
        {
            if (!cppSourceTarget->modFileDeps.empty())
            {
            }
        }
        if (!cppBuildCache.headerUnits.empty())
        {
            oldHeaderUnits.reserve(cppBuildCache.headerUnits.size());
            for (uint64_t i = 0; i < cppBuildCache.headerUnits.size(); ++i)
            {
                oldHeaderUnits.emplace_back(this, cppBuildCache.headerUnits[i].srcFile.node,
                                            string(cppBuildCache.headerUnits[i].smRules.exportName));
                oldHeaderUnits[i].indexInBuildCache = i;
                oldHeaderUnits[i].buildCache = cppBuildCache.headerUnits[i].srcFile;
                oldHeaderUnits[i].smRulesCache = cppBuildCache.headerUnits[i].smRules;
                oldHeaderUnits[i].compileCommandWithToolCache = cppBuildCache.headerUnits[i].compileCommandWithTool;
                oldHeaderUnits[i].type = SM_FILE_TYPE::HEADER_UNIT;
                headerUnitsSet.emplace(&oldHeaderUnits[i]);
            }

            builder.executeMutex.lock();
            for (SMFile *headerUnit : headerUnitsSet)
            {
                builder.updateBTargets.emplace(&headerUnit->realBTargets[1]);
            }
            builder.updateBTargetsSizeGoal += oldHeaderUnits.size();
            builder.addNewTopBeUpdatedTargets(&this->realBTargets[round]);
            isComplete = true;
        }
    }
}

void CppSourceTarget::writeBuildCache(vector<char> &buffer)
{
    cppBuildCache.serialize(buffer);
}

template <typename T> uint32_t findNodeInSourceCache(const vector<T> &sourceCache, const Node *node)
{
    for (uint32_t i = 0; i < sourceCache.size(); ++i)
    {
        Node *node2;
        if constexpr (std::is_same_v<T, BuildCache::Cpp::ModuleFile>)
        {
            node2 = sourceCache[i].srcFile.node;
        }
        else
        {
            node2 = sourceCache[i].node;
        }
        if (node2 == node)
        {
            return i;
        }
    }
    return -1;
}

/// This adjusts build cache during config time so the source and module-files point to the same index as in the config
/// cache.
template <typename T, typename U> void adjustBuildCache(vector<T> &oldCache, const vector<U> &sourceFiles)
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
        if constexpr (std::is_same_v<T, BuildCache::Cpp::ModuleFile>)
        {
            (*newCache)[i].srcFile.node = const_cast<Node *>(sourceFiles[i]->node);
        }
        else
        {
            (*newCache)[i].node = const_cast<Node *>(sourceFiles[i]->node);
        }
    }

    newCache->resize(newlyFounded + oldCache.size());
    oldCache = std::move(*newCache);
}

template <typename T> static const InclNode &getNode(const T &t)
{
    if constexpr (std::is_same_v<T, HuTargetPlusDir>)
    {
        return t.inclNode;
    }
    else if constexpr (std::is_same_v<T, InclNode>)
    {
        return t;
    }
}

template <typename T> void writeIncDirsAtConfigTime(vector<char> &buffer, const vector<T> &include)
{
    writeUint32(buffer, include.size());
    for (auto &elem : include)
    {
        const InclNode &inclNode = getNode(elem);
        writeNode(buffer, inclNode.node);
        writeBool(buffer, inclNode.isStandard);
        writeBool(buffer, inclNode.ignoreHeaderDeps);
        if constexpr (std::is_same_v<T, HuTargetPlusDir>)
        {
            auto &headerUnitNode = static_cast<const HeaderUnitNode &>(inclNode);

            writeUint32(buffer, headerUnitNode.targetCacheIndex);
            writeUint32(buffer, headerUnitNode.headerUnitIndex);
        }
    }
}

template <typename T>
void readInclDirsAtBuildTime(const char *ptr, uint32_t &bytesRead, vector<T> &include, CppSourceTarget *target)
{
    const uint32_t reserveSize = readUint32(ptr, bytesRead);
    include.reserve(reserveSize * sizeof(T) + sizeof(uint32_t));
    for (uint32_t i = 0; i < reserveSize; ++i)
    {
        Node *node = readHalfNode(ptr, bytesRead);
        bool isStandard = readBool(ptr, bytesRead);
        bool ignoreHeaderDeps = readBool(ptr, bytesRead);
        if constexpr (std::is_same_v<T, HuTargetPlusDir>)
        {
            const bool targetCacheIndex = readUint32(ptr, bytesRead);
            const bool headerUnitIndex = readUint32(ptr, bytesRead);
            include.emplace_back(HeaderUnitNode(node, isStandard, ignoreHeaderDeps, targetCacheIndex, headerUnitIndex),
                                 target);
        }
        else
        {
            include.emplace_back(node, isStandard, ignoreHeaderDeps);
        }
    }
}

void CppSourceTarget::writeCacheAtConfigTime(const bool before)
{
    if (before)
    {
        auto *configBuffer = new vector<char>{};
        writeIncDirsAtConfigTime(*configBuffer, reqIncls);
        writeIncDirsAtConfigTime(*configBuffer, useReqIncls);
        writeIncDirsAtConfigTime(*configBuffer, reqHuDirs);
        writeIncDirsAtConfigTime(*configBuffer, useReqHuDirs);

        writeUint32(*configBuffer, srcFileDeps.size());
        for (SourceNode *source : srcFileDeps)
        {
            source->objectNode = Node::getNodeFromNormalizedString(
                myBuildDir->filePath + slashc + source->node->getFileName() + ".o", true, true);

            writeNode(*configBuffer, source->node);
            writeNode(*configBuffer, source->objectNode);
        }

        writeUint32(*configBuffer, modFileDeps.size());
        for (const SMFile *smFile : modFileDeps)
        {
            writeNode(*configBuffer, smFile->node);
            writeBool(*configBuffer, smFile->isInterface);
            writeUint8(*configBuffer, static_cast<uint8_t>(smFile->type));
        }

        writeUint32(*configBuffer, oldHeaderUnits.size());
        for (const SMFile &smFile : oldHeaderUnits)
        {
            writeNode(*configBuffer, smFile.node);
        }

        writeNode(*configBuffer, myBuildDir);

        fileTargetCaches[cacheIndex].configCache = string_view{configBuffer->data(), configBuffer->size()};

        cppBuildCache.deserialize(cacheIndex);

        adjustBuildCache(cppBuildCache.srcFiles, srcFileDeps);
        adjustBuildCache(cppBuildCache.modFiles, modFileDeps);
    }
}

void CppSourceTarget::readConfigCacheAtBuildTime()
{
    const string_view configCache = fileTargetCaches[cacheIndex].configCache;

    uint32_t configRead = 0;
    const char *ptr = configCache.data();

    readInclDirsAtBuildTime(ptr, configRead, reqIncls, this);
    readInclDirsAtBuildTime(ptr, configRead, useReqIncls, this);
    readInclDirsAtBuildTime(ptr, configRead, reqHuDirs, this);
    readInclDirsAtBuildTime(ptr, configRead, useReqHuDirs, this);

    const uint32_t sourceSize = readUint32(ptr, configRead);
    srcFileDeps.reserve(sourceSize);
    for (uint32_t i = 0; i < sourceSize; ++i)
    {
        SourceNode *src = srcFileDeps.emplace_back(new SourceNode(this, readHalfNode(ptr, configRead)));
        src->objectNode = readHalfNode(ptr, configRead);

        addDepNow<0>(*srcFileDeps[i]);
    }

    const uint32_t modSize = readUint32(ptr, configRead);
    modFileDeps.reserve(modSize);
    for (uint32_t i = 0; i < modSize; ++i)
    {
        SMFile *smFile = modFileDeps.emplace_back(new SMFile(this, readHalfNode(ptr, configRead)));
        smFile->isInterface = readBool(ptr, configRead);
        smFile->type = static_cast<SM_FILE_TYPE>(readUint8(ptr, configRead));
        addDepNow<0>(*smFile);
    }

    const uint32_t huSize = readUint32(ptr, configRead);
    if (huSize)
    {
        hasManuallySpecifiedHeaderUnits = true;
    }
    for (uint32_t i = 0; i < huSize; ++i)
    {
        configuration->moduleFilesToTarget.emplace(readHalfNode(ptr, configRead), this);
    }

    myBuildDir = readHalfNode(ptr, configRead);

    if (configRead != configCache.size())
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }
}

string CppSourceTarget::getPrintName() const
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

        for (auto &[fullPath, logicalName] : smFile->smRulesCache.moduleArray)
        {
            if (logicalName == smFile->logicalName)
            {
                printErrorMessage(FORMAT("In target\n{}\nModule\n{}\n can not depend on itself.\n", getPrintName(),
                                         smFile->node->filePath));
            }

            const SMFile *found = nullptr;

            // Even if found, we continue the search to ensure that no two files are providing the same module in
            // the module-files of the target and its dependencies
            RequireNameTargetId req(id, logicalName);

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
                                       getPrintName(), getDependenciesPString(), logicalName, found->node->filePath,
                                       found->node->filePath));
                        }
                        found = found2;
                    }
                }
            }

            if (found)
            {
                smFile->addDepLater(const_cast<SMFile &>(*found));
                if (!smFile->fileStatus && !atomic_ref(smFile->fileStatus).load())
                {
                    if (fullPath != found->objectNode)
                    {
                        atomic_ref(smFile->fileStatus).store(true);
                    }
                }
                BuildCache::Cpp::ModuleFile::SmRules::SingleModuleDep dep;
                dep.logicalName = logicalName;
                dep.fullPath = found->objectNode;
                fullPath = found->objectNode;
            }
            else
            {
                if (isInterface)
                {
                    printErrorMessage(
                        FORMAT("No File in the target\n{}\n provides this module\n{}.\n", getPrintName(), logicalName));
                }
                else
                {
                    printErrorMessage(
                        FORMAT("No File in the target\n{}\n or in its dependencies\n{}\n provides this module\n{}.\n",
                               getPrintName(), getDependenciesPString(), logicalName));
                }
            }
        }
    }
}

string CppSourceTarget::getInfrastructureFlags(const Compiler &compiler)
{
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        string str = "-c /nologo /showIncludes ";
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
        command += getInfrastructureFlags(compiler) + " ";
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
        command += getReducedPath(sourceNode.objectNode->filePath, ccpSettings.objectFile) + " ";
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
        command += getReducedPath(myBuildDir->filePath + slashc + smFile.node->getFileName() + ".smrules",
                                  ccpSettings.objectFile) +
                   " ";
    }

    return command;
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
    return *this;
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
    return *this*/
    ;
}

template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::restore()
{
    objectFileProducer = stored;
    return *this;
}
