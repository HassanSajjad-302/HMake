
#include "CppSourceTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "CacheWriteManager.hpp"
#include "Configuration.hpp"
#include "LOAT.hpp"
#include "Utilities.hpp"
#include "rapidhash/rapidhash.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <utility>

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

HeaderFileOrUnit::HeaderFileOrUnit(SMFile *smFile_, const bool isSystem_)
    : data{.smFile = smFile_}, isUnit(true), isSystem(isSystem_)
{
}

HeaderFileOrUnit::HeaderFileOrUnit(Node *node_, const bool isSystem_)
    : data{.node = node_}, isUnit(false), isSystem(isSystem_)
{
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
        cppBuildCache.deserialize(cacheIndex);
        readConfigCacheAtBuildTime();
    }
}

void CppSourceTarget::getObjectFiles(vector<const ObjectFile *> *objectFiles, LOAT *loat) const
{
    for (const SMFile *objectFile : modFileDeps)
    {
        objectFiles->emplace_back(objectFile);
    }

    for (const SMFile *objectFile : imodFileDeps)
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
        if (configuration->evaluate(TreatModuleAsSource::YES))
        {
            for (const InclNode &inclNode : cppSourceTarget->useReqIncls)
            {
                // Configure-time check.
                actuallyAddInclude(inclNode.node->filePath, true);
                reqIncls.emplace_back(inclNode);
            }
        }
        reqCompilerFlags += cppSourceTarget->useReqCompilerFlags;
        for (const Define &define : cppSourceTarget->useReqCompileDefinitions)
        {
            reqCompileDefinitions.emplace(define);
        }
        reqCompilerFlags += cppSourceTarget->useReqCompilerFlags;
    }
}

CppSourceTarget &CppSourceTarget::initializeUseReqInclsFromReqIncls()
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::YES))
        {
            for (const InclNode &include : reqIncls)
            {
                actuallyAddInclude(include.node->filePath, false, include.isStandard, include.ignoreHeaderDeps);
            }
        }
        else
        {
            useReqHeaderNameMapping = reqHeaderNameMapping;
            for (SMFile *hu : huDeps)
            {
                hu->isUseReqDep = true;
            }
        }
    }

    return *this;
}

void CppSourceTarget::actuallyAddSourceFileConfigTime(Node *node)
{
    if (configuration->evaluate(TreatModuleAsSource::NO))
    {
        printErrorMessage(
            FORMAT("CppSourceTarget {}\n already has module-files but now source-file {} is being added.\n", name,
                   node->filePath));
    }

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

void CppSourceTarget::actuallyAddModuleFileConfigTime(Node *node)
{
    if (configuration->evaluate(TreatModuleAsSource::YES))
    {
        printErrorMessage(
            FORMAT("In CppSourceTarget {}\n module-file {} is being added.\n with TreatModuleAsSource::YES.", name,
                   node->filePath));
    }

    string fileName = node->getFileName();
    const string ext = {fileName.begin() + fileName.find_last_of('.') + 1, fileName.end()};
    if (ext == "cppm" || ext == "ixx")
    {
        for (const SMFile *smFile : imodFileDeps)
        {
            if (smFile->node == node)
            {
                printErrorMessage(
                    FORMAT("Attempting to add {} twice in module-files in cpptarget {}. second insertiion ignored.\n",
                           node->filePath, name));
                return;
            }
        }
        imodFileDeps.emplace_back(new SMFile(this, node));
    }
    else
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
        modFileDeps.emplace_back(new SMFile(this, node));
    }
}

void CppSourceTarget::actuallyAddHeaderUnitConfigTime(Node *node)
{
    if (configuration->evaluate(TreatModuleAsSource::YES))
    {
        printErrorMessage(
            FORMAT("In CppSourceTarget {}\n header-unit {} is being added while TreatModuleAsSource::YES is set.\n",
                   name, node->filePath));
    }

    for (const SMFile *smFile : huDeps)
    {
        if (smFile->node == node)
        {
            printErrorMessage(
                FORMAT("Attempting to add {} twice in header-units in cpptarget {}. second insertiion ignored.\n",
                       node->filePath, name));
            return;
        }
    }
    huDeps.emplace_back(new SMFile(this, node));
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

void CppSourceTarget::actuallyAddMSVCInclude(const string &include, bool addInReq, bool isStandard,
                                             bool ignoreHeaderDeps)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *includeDir = Node::getNodeFromNonNormalizedPath(include, false);
        {
            // Checking for uniqueness and adding in reqIncls or useReqIncls.
            bool found = false;
            vector<InclNode> &inclNodes = addInReq ? reqIncls : useReqIncls;
            for (const InclNode &inclNode : inclNodes)
            {
                if (inclNode.node->myId == includeDir->myId)
                {
                    found = true;
                    printErrorMessage(FORMAT("Include {} is already added.\n", includeDir->filePath));
                    break;
                }
            }

            if (!found)
            {
                inclNodes.emplace_back(includeDir, isStandard, ignoreHeaderDeps);
            }
        }

        if (configuration->evaluate(TreatModuleAsSource::YES))
        {
            return;
        }

        // From the header-units.json in the include-dir, the mentioned header-files are manually added as parsing of
        // header-units.json file fails because of the comments in it.
        flat_hash_set<Node *> mentioned; // those that are mentioned in header-units.json file.
        {
            string headerNames =
                R"(\__msvc_bit_utils.hpp,\__msvc_chrono.hpp,\__msvc_cxx_stdatomic.hpp,\__msvc_filebuf.hpp,\__msvc_format_ucd_tables.hpp,\__msvc_formatter.hpp,\__msvc_heap_algorithms.hpp,\__msvc_int128.hpp,\__msvc_iter_core.hpp,\__msvc_minmax.hpp,\__msvc_ostream.hpp,\__msvc_print.hpp,\__msvc_ranges_to.hpp,\__msvc_ranges_tuple_formatter.hpp,\__msvc_sanitizer_annotate_container.hpp,\__msvc_string_view.hpp,\__msvc_system_error_abi.hpp,\__msvc_threads_core.hpp,\__msvc_tzdb.hpp,\__msvc_xlocinfo_types.hpp,\algorithm,\any,\array,\atomic,\barrier,\bit,\bitset,\cctype,\cerrno,\cfenv,\cfloat,\charconv,\chrono,\cinttypes,\climits,\clocale,\cmath,\codecvt,\compare,\complex,\concepts,\condition_variable,\coroutine,\csetjmp,\csignal,\cstdarg,\cstddef,\cstdint,\cstdio,\cstdlib,\cstring,\ctime,\cuchar,\cwchar,\cwctype,\deque,\exception,\execution,\expected,\filesystem,\format,\forward_list,\fstream,\functional,\future,\generator,\initializer_list,\iomanip,\ios,\iosfwd,\iostream,\iso646.h,\istream,\iterator,\latch,\limits,\list,\locale,\map,\mdspan,\memory,\memory_resource,\mutex,\new,\numbers,\numeric,\optional,\ostream,\print,\queue,\random,\ranges,\ratio,\regex,\scoped_allocator,\semaphore,\set,\shared_mutex,\source_location,\span,\spanstream,\sstream,\stack,\stacktrace,\stdexcept,\stdfloat,\stop_token,\streambuf,\string,\string_view,\strstream,\syncstream,\system_error,\thread,\tuple,\type_traits,\typeindex,\typeinfo,\unordered_map,\unordered_set,\utility,\valarray,\variant,\vector,\xatomic.h,\xatomic_wait.h,\xbit_ops.h,\xcall_once.h,\xcharconv.h,\xcharconv_ryu.h,\xcharconv_ryu_tables.h,\xcharconv_tables.h,\xerrc.h,\xfacet,\xfilesystem_abi.h,\xhash,\xiosbase,\xlocale,\xlocbuf,\xlocinfo,\xlocmes,\xlocmon,\xlocnum,\xloctime,\xmemory,\xnode_handle.h,\xpolymorphic_allocator.h,\xsmf_control.h,\xstring,\xthreads.h,\xtimec.h,\xtr1common,\xtree,\xutility,\ymath.h)";
            uint32_t oldIndex = 0;
            uint32_t index = headerNames.find(',');
            while (index != -1)
            {
                mentioned.emplace(Node::getNodeFromNonNormalizedString(
                    include + string(headerNames.begin() + oldIndex, headerNames.begin() + index), true, false));
                oldIndex = index + 1;
                index = headerNames.find(',', oldIndex);
            }
        }

        for (const auto &p : recursive_directory_iterator(includeDir->filePath))
        {
            if (p.is_regular_file())
            {
                Node *headerNode;
                string *logicalName;
                {
                    auto *str = new string(p.path().string());
                    logicalName = new string{str->data() + includeDir->filePath.size() + 1,
                                             str->size() - includeDir->filePath.size() - 1};
                    lowerCaseOnWindows(str->data(), str->size());
                    headerNode = Node::getHalfNode(*str);

                    if constexpr (os == OS::NT)
                    {
                        for (char &c : *logicalName)
                        {
                            if (c == '\\')
                            {
                                c = '/';
                            }
                        }
                    }
                }

                if (mentioned.contains(headerNode))
                {
                    actuallyAddHeaderUnitConfigTime(headerNode);
                    SMFile *hu = huDeps.back();
                    hu->logicalNames.emplace_back(*logicalName);
                    if (addInReq)
                    {
                        hu->isReqDep = true;
                    }
                    else
                    {
                        hu->isUseReqDep = true;
                    }
                    hu->isSystem = isStandard;
                }
                else
                {
                    if (const auto &[pos, ok] =
                            reqHeaderNameMapping.emplace(*logicalName, HeaderFileOrUnit{headerNode, isStandard});
                        !ok)
                    {
                        bool brekapoint = true;
                        // printErrorMessage(
                        //     FORMAT("There is already a Header-File or Header-Unit entry\n{}\n for name {}\n. Error "
                        //            "happened while adding include {} for target {}.\n",
                        //            pos->second.data.node->filePath, *logicalName, n->filePath, name));
                    }
                }
            }
        }
    }
}

void CppSourceTarget::actuallyAddInclude(const string &include, bool addInReq, bool isStandard, bool ignoreHeaderDeps)
{

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *node = Node::getNodeFromNonNormalizedPath(include, false);
        bool found = false;
        vector<InclNode> &inclNodes = addInReq ? reqIncls : useReqIncls;
        for (const InclNode &inclNode : inclNodes)
        {
            if (inclNode.node->myId == node->myId)
            {
                found = true;
                printErrorMessage(FORMAT("Include {} is already added.\n", node->filePath));
                break;
            }
        }

        if (!found)
        {
            inclNodes.emplace_back(node, isStandard, ignoreHeaderDeps);
        }

        if (configuration->evaluate(TreatModuleAsSource::YES))
        {
            return;
        }

        for (const auto &p : recursive_directory_iterator(node->filePath))
        {
            if (p.is_regular_file())
            {
                auto *str = new string(p.path().string());
                string *logicalName =
                    new string{str->data() + node->filePath.size() + 1, str->size() - node->filePath.size() - 1};
                lowerCaseOnWindows(str->data(), str->size());
                Node *headerNode = Node::getHalfNode(*str);
                if constexpr (os == OS::NT)
                {
                    for (char &c : *logicalName)
                    {
                        if (c == '\\')
                        {
                            c = '/';
                        }
                    }
                }

                if (const auto &[pos, ok] = (addInReq ? reqHeaderNameMapping : useReqHeaderNameMapping)
                                                .emplace(*logicalName, HeaderFileOrUnit{headerNode, isStandard});
                    !ok)
                {
                    /*
                    printErrorMessage(
                        FORMAT("There is already a Header-File or Header-Unit entry\n{}\n for name {}\n. Error "
                               "happened while adding include {} for target {}.\n",
                               pos->second.data.node->filePath, *logicalName, headerNode->filePath, name));
                */
                }
            }
        }
    }
}

void CppSourceTarget::actuallyAddHuDir(const string &include, bool addInReq, bool addInUseReq, bool isStandard,
                                       bool ignoreHeaderDeps)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::YES))
        {
            printErrorMessage(FORMAT("actuallyAddHuDir called for target {}\n for include-dir {}\n. is not "
                                     "allowed in TreatModuleAsSource::YES",
                                     name, include));
        }

        Node *includeDir = Node::getNodeFromNonNormalizedPath(include, false);

        {
            // Checking for uniqueness and adding in reqIncls or useReqIncls.
            bool found = false;
            vector<InclNode> &inclNodes = addInReq ? reqIncls : useReqIncls;
            for (const InclNode &inclNode : inclNodes)
            {
                if (inclNode.node->myId == includeDir->myId)
                {
                    found = true;
                    printErrorMessage(FORMAT("Include {} is already added.\n", includeDir->filePath));
                    break;
                }
            }

            if (!found)
            {
                inclNodes.emplace_back(includeDir, isStandard, ignoreHeaderDeps);
            }
        }

        for (const auto &p : recursive_directory_iterator(includeDir->filePath))
        {
            if (p.is_regular_file())
            {
                Node *headerNode;
                string *logicalName;
                {
                    auto *str = new string(p.path().string());
                    logicalName = new string{str->data() + includeDir->filePath.size() + 1,
                                             str->size() - includeDir->filePath.size() - 1};
                    lowerCaseOnWindows(str->data(), str->size());
                    headerNode = Node::getHalfNode(*str);

                    if constexpr (os == OS::NT)
                    {
                        for (char &c : *logicalName)
                        {
                            if (c == '\\')
                            {
                                c = '/';
                            }
                        }
                    }
                }

                actuallyAddHeaderUnitConfigTime(headerNode);
                SMFile *hu = huDeps.back();
                hu->logicalNames.emplace_back(*logicalName);
                if (addInReq)
                {
                    hu->isReqDep = true;
                }

                if (addInUseReq)
                {
                    hu->isUseReqDep = true;
                }
                hu->isSystem = isStandard;
            }
        }
    }
}

void CppSourceTarget::actuallyAddExtInclude(const string &include, const string &regex, bool isHeaderFile,
                                            bool addInReq, bool isStandard, bool ignoreHeaderDeps)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(TreatModuleAsSource::YES))
        {
            printErrorMessage(FORMAT("actuallyAddExtInclude called for target {}\n for include-dir {}\n. is not "
                                     "allowed in TreatModuleAsSource::YES",
                                     name, include));
        }

        for (const Node *includeDir = Node::getNodeFromNonNormalizedPath(include, false);
             const auto &p : recursive_directory_iterator(includeDir->filePath))
        {
            if (p.is_regular_file() && regex_match(p.path().filename().string(), std::regex(regex)))
            {
                Node *headerNode;
                string *logicalName;
                {
                    auto *str = new string(p.path().string());
                    logicalName = new string{str->data() + includeDir->filePath.size() + 1,
                                             str->size() - includeDir->filePath.size() - 1};
                    lowerCaseOnWindows(str->data(), str->size());
                    headerNode = Node::getHalfNode(*str);

                    if constexpr (os == OS::NT)
                    {
                        for (char &c : *logicalName)
                        {
                            if (c == '\\')
                            {
                                c = '/';
                            }
                        }
                    }
                }

                if (isHeaderFile)
                {
                    if (const auto &[pos, ok] = (addInReq ? reqHeaderNameMapping : useReqHeaderNameMapping)
                                                    .emplace(*logicalName, HeaderFileOrUnit{headerNode, isStandard});
                        !ok)
                    {
                        printErrorMessage(
                            FORMAT("There is already a Header-File or Header-Unit entry\n{}\n for name {}\n. Error "
                                   "happened while adding include {} for target {}.\n",
                                   pos->second.data.node->filePath, *logicalName, headerNode->filePath, name));
                    }
                }
                else
                {
                    actuallyAddHeaderUnitConfigTime(headerNode);
                    SMFile *hu = huDeps.back();
                    hu->logicalNames.emplace_back(*logicalName);
                    if (addInReq)
                    {
                        hu->isReqDep = true;
                    }
                    else
                    {
                        hu->isUseReqDep = true;
                    }
                    hu->isSystem = isStandard;
                }
            }
        }
    }
}

void CppSourceTarget::updateBTarget(Builder &builder, const unsigned short round, bool &isComplete)
{
    if (!round)
    {
        if (realBTargets[0].updateStatus == UpdateStatus::NEEDS_UPDATE)
        {
            // This is necessary since objectFile->outputFileNode is not updated once after it is compiled.
            realBTargets[0].assignFileStatusToDependents();
        }
    }
    else if (round == 1)
    {
        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            for (CppSourceTarget *t : reqDeps)
            {
                for (const auto &[n, t] : t->useReqNodesType)
                {
                    if (const auto &[it, ok] = reqNodesType.emplace(n, t); !ok)
                    {
                        if (it->second != t)
                        {
                            printErrorMessage(FORMAT(
                                "In target {} and its dependencies, node {} is specified as {} and {} as well.\n", name,
                                n->filePath, static_cast<uint8_t>(t), static_cast<uint8_t>(it->second)));
                        }
                    }
                }
            }

            writeCacheAtConfigTime();

            for (uint32_t i = 0; i < modFileDeps.size(); ++i)
            {
                setHeaderStatusChanged(cppBuildCache.modFiles[i]);
            }

            for (uint32_t i = 0; i < imodFileDeps.size(); ++i)
            {
                setHeaderStatusChanged(cppBuildCache.imodFiles[i]);
            }

            for (SMFile *hu : huDeps)
            {
                setHeaderStatusChanged(cppBuildCache.headerUnits[hu->indexInBuildCache]);
            }
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

        for (uint32_t i = 0; i < srcFileDeps.size(); ++i)
        {
            srcFileDeps[i]->initializeBuildCache(i);
        }
        for (uint32_t i = 0; i < modFileDeps.size(); ++i)
        {
            modFileDeps[i]->initializeBuildCache(cppBuildCache.modFiles[i], i);
        }
        for (uint32_t i = 0; i < imodFileDeps.size(); ++i)
        {
            imodFileDeps[i]->initializeBuildCache(cppBuildCache.imodFiles[i], i);
        }
        for (uint32_t i = 0; i < huDeps.size(); ++i)
        {
            if (huDeps[i])
            {
                huDeps[i]->initializeBuildCache(cppBuildCache.headerUnits[i], i);
            }
        }

        setSourceCompileCommandPrintFirstHalf();
        for (const CppSourceTarget *cppSourceTarget : reqDeps)
        {
            if (!cppSourceTarget->modFileDeps.empty())
            {
            }
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

void writeHeaderFilesOrUnitsAtConfigTime(vector<char> &buffer,
                                         flat_hash_map<string_view, HeaderFileOrUnit> &headerNameMapping)
{
    writeUint32(buffer, headerNameMapping.size());
    for (const auto &[s, h] : headerNameMapping)
    {
        assert(!h.isUnit && "HeaderFileOrUnit can not be HeaderUnit at config-time\n");
        writeStringView(buffer, s);
        writeNode(buffer, h.data.node);
        writeBool(buffer, h.isSystem);
    }
}

void readHeaderFilesAtBuildTime(const char *ptr, uint32_t &bytesRead,
                                flat_hash_map<string_view, HeaderFileOrUnit> &headerNameMapping)
{
    const uint32_t includeSize = readUint32(ptr, bytesRead);
    for (uint32_t i = 0; i < includeSize; ++i)
    {
        string_view name = readStringView(ptr, bytesRead);
        Node *node = readHalfNode(ptr, bytesRead);
        const bool isStandard = readBool(ptr, bytesRead);
        headerNameMapping.emplace(name, HeaderFileOrUnit{node, isStandard});
    }
}

void readHeaderUnitesAtBuildTime(const char *ptr, uint32_t &bytesRead,
                                 flat_hash_map<string_view, HeaderFileOrUnit> &headerNameMapping,
                                 vector<SMFile> &headerUnits)
{
    const uint32_t includeSize = readUint32(ptr, bytesRead);
    for (uint32_t i = 0; i < includeSize; ++i)
    {
        string_view name = readStringView(ptr, bytesRead);
        const uint32_t index = readUint32(ptr, bytesRead);
        const bool isStandard = readBool(ptr, bytesRead);
        headerNameMapping.emplace(name, HeaderFileOrUnit{&headerUnits[index], isStandard});
    }
}

void CppSourceTarget::setHeaderStatusChanged(BuildCache::Cpp::ModuleFile &modCache)
{
    for (Node *node : modCache.srcFile.headerFiles)
    {
        if (auto it = reqNodesType.find(node); it != reqNodesType.end())
        {
            if (it->second != FileType::HEADER_FILE)
            {
                modCache.smRules.headerStatusChanged = true;
                return;
            }
        }
        else
        {
            modCache.smRules.headerStatusChanged = true;
            return;
        }
    }

    for (BuildCache::Cpp::ModuleFile::SmRules::SingleHeaderUnitDep &huDep : modCache.smRules.headerUnitArray)
    {
        if (auto it = reqNodesType.find(huDep.node); it != reqNodesType.end())
        {
            if (it->second != FileType::HEADER_UNIT)
            {
                modCache.smRules.headerStatusChanged = true;
                return;
            }
        }
        else
        {
            modCache.smRules.headerStatusChanged = true;
            return;
        }
    }
}

void CppSourceTarget::writeCacheAtConfigTime()
{
    cppBuildCache.deserialize(cacheIndex);
    auto *configBuffer = new vector<char>{};

    writeUint32(*configBuffer, srcFileDeps.size());
    for (SourceNode *source : srcFileDeps)
    {
        source->objectNode = Node::getNodeFromNormalizedString(
            myBuildDir->filePath + slashc + source->node->getFileName() + ".o", true, true);

        writeNode(*configBuffer, source->node);
        writeNode(*configBuffer, source->objectNode);
    }

    writeUint32(*configBuffer, modFileDeps.size());
    for (SMFile *smFile : modFileDeps)
    {
        smFile->objectNode = Node::getNodeFromNormalizedString(
            myBuildDir->filePath + slashc + smFile->node->getFileName() + ".o", true, true);
        writeNode(*configBuffer, smFile->node);
        writeNode(*configBuffer, smFile->objectNode);
    }

    writeUint32(*configBuffer, imodFileDeps.size());
    for (SMFile *smFile : imodFileDeps)
    {
        smFile->objectNode = Node::getNodeFromNormalizedString(
            myBuildDir->filePath + slashc + smFile->node->getFileName() + ".o", true, true);
        smFile->interfaceNode = Node::getNodeFromNormalizedString(
            myBuildDir->filePath + slashc + smFile->node->getFileName() + ".ifc", true, true);
        writeNode(*configBuffer, smFile->node);
        writeNode(*configBuffer, smFile->objectNode);
        writeNode(*configBuffer, smFile->interfaceNode);
    }

    writeUint32(*configBuffer, huDeps.size());
    for (SMFile *hu : huDeps)
    {
        uint32_t index = findNodeInSourceCache(cppBuildCache.headerUnits, hu->node);
        if (index == -1)
        {
            index = cppBuildCache.headerUnits.size();
            cppBuildCache.headerUnits.emplace_back();
            cppBuildCache.headerUnits[index].srcFile.node = const_cast<Node *>(hu->node);
        }
        hu->indexInBuildCache = index;
        hu->interfaceNode = Node::getNodeFromNormalizedString(
            myBuildDir->filePath + slashc + hu->node->getFileName() + ".ifc", true, true);

        writeUint32(*configBuffer, hu->indexInBuildCache);
        writeNode(*configBuffer, hu->node);
        writeNode(*configBuffer, hu->interfaceNode);
        const uint32_t logicalNamesSize = hu->logicalNames.size();
        writeUint32(*configBuffer, logicalNamesSize);
        for (uint32_t i = 0; i < logicalNamesSize; ++i)
        {
            writeStringView(*configBuffer, hu->logicalNames[i]);
        }
        writeBool(*configBuffer, hu->isReqDep);
        writeBool(*configBuffer, hu->isUseReqDep);
        writeBool(*configBuffer, hu->isSystem);
    }

    writeNode(*configBuffer, myBuildDir);

    if (configuration->evaluate(TreatModuleAsSource::YES))
    {
        writeIncDirsAtConfigTime(*configBuffer, reqIncls);
        writeIncDirsAtConfigTime(*configBuffer, useReqIncls);
    }
    else
    {
        writeHeaderFilesOrUnitsAtConfigTime(*configBuffer, reqHeaderNameMapping);
        writeHeaderFilesOrUnitsAtConfigTime(*configBuffer, useReqHeaderNameMapping);
    }

    fileTargetCaches[cacheIndex].configCache = string_view{configBuffer->data(), configBuffer->size()};

    adjustBuildCache(cppBuildCache.srcFiles, srcFileDeps);
    adjustBuildCache(cppBuildCache.modFiles, modFileDeps);
    adjustBuildCache(cppBuildCache.imodFiles, imodFileDeps);
}

void CppSourceTarget::readConfigCacheAtBuildTime()
{
    const string_view configCache = fileTargetCaches[cacheIndex].configCache;

    uint32_t configRead = 0;
    const char *ptr = configCache.data();

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
        smFile->objectNode = readHalfNode(ptr, configRead);
        smFile->type = SM_FILE_TYPE::PRIMARY_IMPLEMENTATION;

        addDepNow<0>(*smFile);
    }

    const uint32_t imodSize = readUint32(ptr, configRead);
    imodFileDeps.reserve(imodSize);
    for (uint32_t i = 0; i < imodSize; ++i)
    {
        SMFile *smFile = imodFileDeps.emplace_back(new SMFile(this, readHalfNode(ptr, configRead)));
        smFile->objectNode = readHalfNode(ptr, configRead);
        smFile->interfaceNode = readHalfNode(ptr, configRead);
        string str = smFile->node->getFileStem();
        smFile->type = str.contains('-') ? SM_FILE_TYPE::PARTITION_EXPORT : SM_FILE_TYPE::PRIMARY_EXPORT;
        smFile->logicalName = str;
        imodNames.emplace(std::move(str), smFile);

        addDepNow<0>(*smFile);
    }

    huDeps.resize(cppBuildCache.headerUnits.size());

    const uint32_t huSize = readUint32(ptr, configRead);
    for (uint32_t i = 0; i < huSize; ++i)
    {
        const uint32_t indexInBuildCache = readUint32(ptr, configRead);
        SMFile *hu = huDeps[indexInBuildCache] = new SMFile(this, readHalfNode(ptr, configRead));
        hu->indexInBuildCache = indexInBuildCache;

        hu->interfaceNode = readHalfNode(ptr, configRead);
        const uint32_t logicalNamesSize = readUint32(ptr, configRead);
        hu->logicalNames.reserve(logicalNamesSize);
        for (uint32_t j = 0; j < logicalNamesSize; ++j)
        {
            hu->logicalNames.emplace_back(readStringView(ptr, configRead));
        }
        hu->isReqDep = readBool(ptr, configRead);
        hu->isUseReqDep = readBool(ptr, configRead);
        hu->isSystem = readBool(ptr, configRead);

        if (hu->isReqDep)
        {
            for (const string &str : hu->logicalNames)
            {
                reqHeaderNameMapping.emplace(str, HeaderFileOrUnit(hu, hu->isSystem));
            }
        }

        if (hu->isUseReqDep)
        {
            for (const string &str : hu->logicalNames)
            {
                useReqHeaderNameMapping.emplace(str, HeaderFileOrUnit(hu, hu->isSystem));
            }
        }

        hu->type = SM_FILE_TYPE::HEADER_UNIT;
        addDepNow<0>(*hu);
    }

    myBuildDir = readHalfNode(ptr, configRead);

    if (configuration->evaluate(TreatModuleAsSource::YES))
    {
        readInclDirsAtBuildTime(ptr, configRead, reqIncls, this);
        readInclDirsAtBuildTime(ptr, configRead, useReqIncls, this);
    }
    else
    {
        readHeaderFilesAtBuildTime(ptr, configRead, reqHeaderNameMapping);
        readHeaderFilesAtBuildTime(ptr, configRead, useReqHeaderNameMapping);
    }

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
    if (configuration->evaluate(TreatModuleAsSource::YES))
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
                actuallyAddModuleFileConfigTime(node);
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
        compileCommand +=
            flags.LANG + flags.OPTIONS + flags.OPTIONS_COMPILE + flags.OPTIONS_COMPILE_CPP + flags.DEFINES_COMPILE_CPP;
    }
    else if (compiler.bTFamily == BTFamily::MSVC)
    {
        if (compiler.btSubFamily == BTSubFamily::CLANG)
        {
            compileCommand += "-nostdinc ";
        }
        compileCommand +=
            flags.CPP_FLAGS_COMPILE_CPP + flags.CPP_FLAGS_COMPILE + flags.OPTIONS_COMPILE + flags.OPTIONS_COMPILE_CPP;
    }

    auto getIncludeFlag = [&compiler](bool isStandard) {
        string str;
        if (compiler.bTFamily == BTFamily::MSVC)
        {
            if (isStandard)
            {
                str += "/external:I ";
            }
            else
            {
                str += "/I ";
            }
        }
        else
        {
            if (isStandard)
            {
                // str += "-isystem ";
                str += "-I ";
            }
            else
            {
                str += "-I ";
            }
        }
        return str;
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

    if (compiler.bTFamily == BTFamily::MSVC)
    {
        compileCommand += "/external:W0 ";
    }

    auto it = reqIncls.begin();
    for (unsigned short i = 0; i < reqIncSizeBeforePopulate; ++i)
    {
        compileCommand.append(getIncludeFlag(it->isStandard) + addQuotes(it->node->filePath) + " ");
        ++it;
    }

    btree_set<string> includes;

    for (; it != reqIncls.end(); ++it)
    {
        includes.emplace(getIncludeFlag(it->isStandard) + addQuotes(it->node->filePath) + ' ');
    }

    for (const string &include : includes)
    {
        compileCommand.append(include);
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
            sourceCompileCommandPrintFirstHalf += flags.CPP_FLAGS_COMPILE_CPP + flags.CPP_FLAGS_COMPILE +
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

string CppSourceTarget::getDependenciesString() const
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
                                       getPrintName(), getDependenciesString(), logicalName, found->node->filePath,
                                       found->node->filePath));
                        }
                        found = found2;
                    }
                }
            }

            if (found)
            {
                /*smFile->addDepLater(const_cast<SMFile &>(*found));
                if (!smFile->fileStatus && !atomic_ref(smFile->fileStatus).load())
                {
                    if (fullPath != found->objectNode)
                    {
                        atomic_ref(smFile->fileStatus).store(true);
                    }
                }
                BuildCache::Cpp::ModuleFile::SmRules::SingleModuleDep dep;
                dep.logicalName = logicalName;
                dep.node = found->objectNode;
                fullPath = found->objectNode;*/
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
                               getPrintName(), getDependenciesString(), logicalName));
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
            command += " /nologo /showIncludes /scanDependencies ";
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
