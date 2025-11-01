
#include "CppSourceTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "CacheWriteManager.hpp"
#include "ConfigurationAssign.hpp"
#include "LOAT.hpp"
#include "Utilities.hpp"
#include "rapidhash/rapidhash.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <utility>

using std::filesystem::create_directories, std::filesystem::directory_iterator,
    std::filesystem::recursive_directory_iterator, std::ifstream, std::ofstream, std::regex, std::regex_error;

void CppSourceTarget::readModuleMapFromDir(const string &dir)
{
    const string modeStrs[] = {
        "public-header-files",  "private-header-files",   "interface-header-files", "public-header-units",
        "private-header-units", "interface-header-units", "interface-files",        "module-files",
    };

    string str = fileToString(dir + "module-map.txt");
    uint32_t start = 0;
    int currentModeIndex = -1;
    string_view pendingLogicalName;

    for (uint64_t i = str.find('\n', start); i != string::npos; start = i + 1, i = str.find('\n', start))
    {
        string_view line = str.substr(start, i - start);

        // Skip comments and empty lines
        if (line.starts_with("//") || line.empty())
        {
            continue;
        }

        // Check if this line is a mode declaration
        bool isModeDeclaration = false;
        for (int newModeIndex = 0; newModeIndex < 8; ++newModeIndex)
        {
            if (line == modeStrs[newModeIndex])
            {
                // Check that modes appear in order
                if (currentModeIndex >= newModeIndex)
                {
                    if (currentModeIndex == newModeIndex)
                    {
                        printErrorMessage(FORMAT("Error: mode {} supplied twice", modeStrs[newModeIndex]));
                    }
                    printErrorMessage(FORMAT("Error: mode {} out of order", modeStrs[newModeIndex]));
                }

                if (!pendingLogicalName.empty())
                {
                    printErrorMessage(FORMAT("Error: incomplete entry - missing file path for logical name: {}",
                                             string(pendingLogicalName)));
                }

                currentModeIndex = newModeIndex;
                isModeDeclaration = true;
                break;
            }
        }

        if (isModeDeclaration)
        {
            continue;
        }

        // Parse data based on current mode
        if (currentModeIndex == -1)
        {
            printErrorMessage(FORMAT("Error: data found before any mode declaration"));
            return;
        }

        if (pendingLogicalName.empty())
        {
            pendingLogicalName = line;
            continue;
        }

        Node *node = Node::getNodeFromNonNormalizedString(string(line), true, true);
        if (node->fileType == file_type::not_found)
        {
            printErrorMessage(FORMAT("Error: file {}\n provided in mode {}\n does not exists while parsing {}\n",
                                     node->filePath, modeStrs[currentModeIndex], dir + "module-map.txt"));
        }

        if (currentModeIndex == 0)
        {
            addHeaderUnit(string(pendingLogicalName), node, false, true, true);
        }
        else if (currentModeIndex == 1)
        {
            addHeaderUnit(string(pendingLogicalName), node, false, true, false);
        }
        else if (currentModeIndex == 2)
        {
            addHeaderUnit(string(pendingLogicalName), node, false, false, true);
        }
        else if (currentModeIndex == 3)
        {
            addHeaderFile(string(pendingLogicalName), node, false, true, true);
        }
        else if (currentModeIndex == 4)
        {
            addHeaderFile(string(pendingLogicalName), node, false, true, false);
        }
        else if (currentModeIndex == 5)
        {
            addHeaderFile(string(pendingLogicalName), node, false, false, true);
        }
        else if (currentModeIndex == 6)
        {
            actuallyAddModuleFileConfigTime(node, string(pendingLogicalName));
        }
        else if (currentModeIndex == 7)
        {
            actuallyAddModuleFileConfigTime(node, "");
        }
    }
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
    isSystem = configuration->evaluate(SystemTarget::YES);
    ignoreHeaderDeps = configuration->evaluate(IgnoreHeaderDeps::YES);

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(BigHeaderUnit::YES))
        {
            publicBigHu.emplace_back(nullptr);
            privateBigHu.emplace_back(nullptr);
            interfaceBigHu.emplace_back(nullptr);
        }
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

void CppSourceTarget::updateBuildCache(void *ptr, string &outputStr, string &errorStr, bool &buildCacheModified)
{
    static_cast<SourceNode *>(ptr)->updateBuildCache(outputStr, errorStr, buildCacheModified);
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
                actuallyAddInclude(true, inclNode.node, true, false);
                reqIncls.emplace_back(inclNode);
            }
        }
        reqCompilerFlags += cppSourceTarget->useReqCompilerFlags;
        for (const Define &define : cppSourceTarget->useReqCompileDefinitions)
        {
            reqCompileDefinitions.emplace(define);
        }
    }
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

void CppSourceTarget::actuallyAddModuleFileConfigTime(Node *node, string exportName)
{
    if (configuration->evaluate(TreatModuleAsSource::YES))
    {
        printErrorMessage(
            FORMAT("In CppSourceTarget {}\n module-file {} is being added.\n with TreatModuleAsSource::YES.", name,
                   node->filePath));
    }

    if (exportName.empty())
    {
        string fileName = node->getFileName();
        if (const string ext = {fileName.begin() + fileName.find_last_of('.') + 1, fileName.end()};
            ext == "cppm" || ext == "ixx")
        {
            exportName = node->getFileStem();
        }
    }

    if (exportName.empty())
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
    else
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
        imodFileDeps[imodFileDeps.size() - 1]->logicalNames.emplace_back(exportName);
    }
}

void CppSourceTarget::emplaceInHeaderNameMapping(string_view headerName, HeaderFileOrUnit type, bool addInReq,
                                                 const bool suppressError)
{
    if (const auto &[it, ok] = (addInReq ? reqHeaderNameMapping : useReqHeaderNameMapping).emplace(headerName, type);
        ok)
    {
        if (!type.isUnit)
        {
            ++(addInReq ? reqHeaderFilesSize : useReqHeaderFilesSize);
        }
    }
    else if (!suppressError)
    {
        printErrorMessage(
            FORMAT("In CppSourceTarget {}\nheaderNameMapping already has headerName {}.\n", name, string(headerName)));
    }
}

void CppSourceTarget::emplaceInNodesType(const Node *node, FileType type, bool addInReq)
{
    if (node->filePath.contains("yvals_core.h"))
    {
        bool breakpoint = true;
    }
    if (const auto &[it, ok] = (addInReq ? reqNodesType : useReqNodesType).emplace(node, type);
        !ok && it->second != type)
    {
        printErrorMessage(FORMAT("In CppSourceTarget {}\nnodesTypeMap already has Node {} but with different type\n",
                                 name, node->filePath));
    }
}

void CppSourceTarget::makeHeaderFileAsUnit(const string &logicalName, bool addInReq, bool addInUseReq)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        return;
    }

    if (configuration->evaluate(TreatModuleAsSource::YES))
    {
        return;
    }

    string *p = new string(logicalName);
    lowerCaseOnWindows(p->data(), p->size());

    Node *headerNode = nullptr;
    if (addInReq)
    {
        if (const auto &it = reqHeaderNameMapping.find(*p); it == reqHeaderNameMapping.end())
        {
            printErrorMessage(FORMAT("Could not find the header {}\n in removeHeaderFile in target {}\n", *p, name));
        }
        else
        {
            headerNode = it->second.data.node;
            reqHeaderNameMapping.erase(it);
            --reqHeaderFilesSize;
        }

        if (const auto &it = reqNodesType.find(headerNode); it == reqNodesType.end())
        {
            HMAKE_HMAKE_INTERNAL_ERROR
        }
        else
        {
            reqNodesType.erase(headerNode);
        }
    }

    if (addInUseReq)
    {
        if (const auto &it = useReqHeaderNameMapping.find(*p); it == useReqHeaderNameMapping.end())
        {
            printErrorMessage(FORMAT("Could not find the header {}\n in removeHeaderFile in target {}\n", *p, name));
        }
        else
        {
            headerNode = it->second.data.node;
            useReqHeaderNameMapping.erase(it);
            --useReqHeaderFilesSize;
        }

        if (const auto &it = useReqNodesType.find(headerNode); it == useReqNodesType.end())
        {
            HMAKE_HMAKE_INTERNAL_ERROR
        }
        else
        {
            useReqNodesType.erase(headerNode);
        }
    }

    addHeaderUnit(*p, headerNode, true, addInReq, addInUseReq);
}

void CppSourceTarget::removeHeaderFile(const string &logicalName, const bool addInReq, const bool addInUseReq)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        return;
    }

    string *p = new string(logicalName);
    lowerCaseOnWindows(p->data(), p->size());

    if (addInReq)
    {
        Node *headerNode = nullptr;
        if (const auto &it = reqHeaderNameMapping.find(*p); it == reqHeaderNameMapping.end())
        {
            printErrorMessage(FORMAT("Could not find the header {}\n in removeHeaderFile in target {}\n", *p, name));
        }
        else
        {
            headerNode = it->second.data.node;
            reqHeaderNameMapping.erase(it);
            --reqHeaderFilesSize;
        }

        if (const auto &it = reqNodesType.find(headerNode); it == reqNodesType.end())
        {
            HMAKE_HMAKE_INTERNAL_ERROR
        }
        else
        {
            reqNodesType.erase(headerNode);
        }
    }

    if (addInUseReq)
    {
        Node *headerNode = nullptr;
        if (const auto &it = useReqHeaderNameMapping.find(*p); it == useReqHeaderNameMapping.end())
        {
            printErrorMessage(FORMAT("Could not find the header {}\n in removeHeaderFile in target {}\n", *p, name));
        }
        else
        {
            headerNode = it->second.data.node;
            useReqHeaderNameMapping.erase(it);
            --useReqHeaderFilesSize;
        }

        if (const auto &it = useReqNodesType.find(headerNode); it == useReqNodesType.end())
        {
            HMAKE_HMAKE_INTERNAL_ERROR
        }
        else
        {
            useReqNodesType.erase(headerNode);
        }
    }
}

void CppSourceTarget::removeHeaderUnit(const Node *headerNode, const string &logicalName, const bool addInReq,
                                       const bool addInUseReq)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        return;
    }

    string *p = new string(logicalName);
    lowerCaseOnWindows(p->data(), p->size());
    bool found = false;
    if (configuration->evaluate(BigHeaderUnit::YES))
    {
        SMFile *bigHu = nullptr;
        if (addInReq && addInUseReq)
        {
            bigHu = publicBigHu[publicBigHu.size() - 1];
        }
        else if (addInReq)
        {
            bigHu = privateBigHu[privateBigHu.size() - 1];
        }
        else if (addInUseReq)
        {
            bigHu = interfaceBigHu[interfaceBigHu.size() - 1];
        }

        if (!std::erase(bigHu->logicalNames, *p))
        {
            printErrorMessage(
                FORMAT("Could not find logical-name {} to remove in public-header-unit in target {}\n.", *p, name));
        }
        if (!bigHu->composingHeaders.erase(*p))
        {
            printErrorMessage(
                FORMAT("Could not find composing-header {} to remove in public-header-unit in target {}\n.", *p, name));
        }
        if (bigHu->logicalNames.empty())
        {
            delete bigHu;
        }
    }
    else
    {
        for (auto it = huDeps.begin(); it != huDeps.end(); ++it)
        {
            if ((*it)->node == headerNode)
            {
                huDeps.erase(it);
                found = true;
                break;
            }
        }
    }

    if (!found)
    {
        printErrorMessage(
            FORMAT("Could not find the header-unit {}\n with logical-name {}\n in target {}\n to delete.\n",
                   headerNode->filePath, logicalName, name));
    }

    if (addInReq)
    {
        reqNodesType.erase(headerNode);
    }

    if (addInUseReq)
    {
        useReqNodesType.erase(headerNode);
    }
}

void CppSourceTarget::addHeaderFile(const string &logicalName, const Node *headerFile, const bool suppressError,
                                    const bool addInReq, const bool addInUseReq)
{
    string *p = new string(logicalName);
    if (headerFile->filePath.contains("workaround.hpp"))
    {
        bool breakpoint = true;
    }
    lowerCaseOnWindows(p->data(), p->size());
    if (addInReq)
    {
        emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{const_cast<Node *>(headerFile), isSystem}, true, suppressError);
        emplaceInNodesType(headerFile, FileType::HEADER_FILE, true);
    }

    if (addInUseReq)
    {
        emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{const_cast<Node *>(headerFile), isSystem}, false,
                                   suppressError);
        emplaceInNodesType(headerFile, FileType::HEADER_FILE, false);
    }
}

void CppSourceTarget::addHeaderUnit(const string &logicalName, const Node *headerUnit, bool suppressError,
                                    const bool addInReq, const bool addInUseReq)
{
    if (logicalName.empty())
    {
        bool breakpoint = true;
    }

    string *p = new string(logicalName);
    lowerCaseOnWindows(p->data(), p->size());

    SMFile *hu = nullptr;

    if (configuration->evaluate(BigHeaderUnit::YES))
    {
        if (addInReq && addInUseReq)
        {
            const uint32_t index = publicBigHu.size() - 1;
            if (!publicBigHu[index])
            {
                const string str(myBuildDir->filePath + slashc + std::to_string(index) + "public-" +
                                 std::to_string(cacheIndex) + ".hpp");
                const Node *bigHuNode = Node::getNodeFromNonNormalizedString(str, true, true);
                publicBigHu[index] = new SMFile(this, bigHuNode);
                emplaceInNodesType(bigHuNode, FileType::HEADER_UNIT, false);
            }
            hu = publicBigHu[index];
        }
        else if (addInReq)
        {
            const uint32_t index = privateBigHu.size() - 1;
            if (!privateBigHu[index])
            {
                const string str(myBuildDir->filePath + slashc + std::to_string(index) + "private-" +
                                 std::to_string(cacheIndex) + ".hpp");
                const Node *bigHuNode = Node::getNodeFromNonNormalizedString(str, true, true);
                privateBigHu[index] = new SMFile(this, bigHuNode);
                emplaceInNodesType(bigHuNode, FileType::HEADER_UNIT, false);
            }
            hu = privateBigHu[index];
        }
        else if (addInUseReq)
        {
            const uint32_t index = interfaceBigHu.size() - 1;
            if (!interfaceBigHu[index])
            {
                const string str(myBuildDir->filePath + slashc + std::to_string(index) + "interface-" +
                                 std::to_string(cacheIndex) + ".hpp");
                const Node *bigHuNode = Node::getNodeFromNonNormalizedString(str, true, true);
                interfaceBigHu[index] = new SMFile(this, bigHuNode);
                emplaceInNodesType(bigHuNode, FileType::HEADER_UNIT, false);
            }
            hu = interfaceBigHu[index];
        }

        // if suppressError is to be added, then emplace here needs to be conditioned.
        hu->logicalNames.emplace_back(*p);
        hu->composingHeaders.emplace(logicalName, const_cast<Node *>(headerUnit));

        if (addInReq)
        {
            emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{hu, false}, true, suppressError);
            emplaceInNodesType(headerUnit, FileType::HEADER_FILE, true);
            hu->isReqDep = true;
        }

        if (addInUseReq)
        {
            emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{hu, false}, false, suppressError);
            emplaceInNodesType(headerUnit, FileType::HEADER_FILE, false);
            hu->isUseReqDep = true;
        }
    }
    else
    {
        for (const SMFile *smFile : huDeps)
        {
            if (smFile->node == headerUnit)
            {
                printErrorMessage(FORMAT("Attempting to add {} twice in header-units in cpptarget {}.\n",
                                         headerUnit->filePath, name));
            }
        }

        hu = huDeps.emplace_back(new SMFile(this, headerUnit));

        // if suppressError is to be added, then emplace here needs to be conditioned.
        hu->logicalNames.emplace_back(*p);

        if (addInReq)
        {
            emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{hu, false}, true, suppressError);
            emplaceInNodesType(headerUnit, FileType::HEADER_UNIT, true);
            hu->isReqDep = true;
        }

        if (addInUseReq)
        {
            emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{hu, false}, false, suppressError);
            emplaceInNodesType(headerUnit, FileType::HEADER_UNIT, false);
            hu->isUseReqDep = true;
        }
    }
}

void CppSourceTarget::addHeaderUnitOrFileDir(const Node *includeDir, const string &prefix, const bool isHeaderFile,
                                             const string &regexStr, const bool addInReq, const bool addInUseReq)
{
    if (configuration->evaluate(TreatModuleAsSource::YES))
    {
        return;
    }

    for (const auto &p : recursive_directory_iterator(includeDir->filePath))
    {
        if (p.is_regular_file() &&
            (regexStr.empty() || regex_match(p.path().filename().string(), std::regex(regexStr))))
        {
            Node *headerNode;
            string *logicalName;
            {
                string str = p.path().string();
                lowerCaseOnWindows(str.data(), str.size());
                // logicalName is a string as it is stored as string_view in reqHeaderNameMapping. reqHeaderNameMapping
                // has string_view so it is fast initialized at build-time.
                logicalName = new string(prefix + string{str.data() + includeDir->filePath.size() + 1,
                                                         str.size() - includeDir->filePath.size() - 1});
                headerNode = Node::getHalfNode(str);

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
                addHeaderFile(*logicalName, headerNode, false, addInReq, addInUseReq);
            }
            else
            {
                addHeaderUnit(*logicalName, headerNode, false, addInReq, addInUseReq);
            }
        }
    }
}

void CppSourceTarget::addHeaderUnitOrFileDirMSVC(const Node *includeDir, bool isHeaderFile, const bool useMentioned,
                                                 const bool addInReq, const bool addInUseReq, const bool isStandard,
                                                 bool ignoreHeaderDeps)
{
    if (configuration->evaluate(TreatModuleAsSource::YES))
    {
        return;
    }

    // From the header-units.json in the include-dir, the mentioned header-files are manually added as parsing of
    // header-units.json file fails because of the comments in it.
    flat_hash_set<Node *> mentioned; // those that are mentioned in header-units.json file.
    if (useMentioned)
    {
        string headerNames =
            R"(\__msvc_bit_utils.hpp,\__msvc_chrono.hpp,\__msvc_cxx_stdatomic.hpp,\__msvc_filebuf.hpp,\__msvc_format_ucd_tables.hpp,\__msvc_formatter.hpp,\__msvc_heap_algorithms.hpp,\__msvc_int128.hpp,\__msvc_iter_core.hpp,\__msvc_minmax.hpp,\__msvc_ostream.hpp,\__msvc_print.hpp,\__msvc_ranges_to.hpp,\__msvc_ranges_tuple_formatter.hpp,\__msvc_sanitizer_annotate_container.hpp,\__msvc_string_view.hpp,\__msvc_system_error_abi.hpp,\__msvc_threads_core.hpp,\__msvc_tzdb.hpp,\__msvc_xlocinfo_types.hpp,\algorithm,\any,\array,\atomic,\barrier,\bit,\bitset,\cctype,\cerrno,\cfenv,\cfloat,\charconv,\chrono,\cinttypes,\climits,\clocale,\cmath,\codecvt,\compare,\complex,\concepts,\condition_variable,\coroutine,\csetjmp,\csignal,\cstdarg,\cstddef,\cstdint,\cstdio,\cstdlib,\cstring,\ctime,\cuchar,\cwchar,\cwctype,\deque,\exception,\execution,\expected,\filesystem,\format,\forward_list,\fstream,\functional,\future,\generator,\initializer_list,\iomanip,\ios,\iosfwd,\iostream,\iso646.h,\istream,\iterator,\latch,\limits,\list,\locale,\map,\mdspan,\memory,\memory_resource,\mutex,\new,\numbers,\numeric,\optional,\ostream,\print,\queue,\random,\ranges,\ratio,\regex,\scoped_allocator,\semaphore,\set,\shared_mutex,\source_location,\span,\spanstream,\sstream,\stack,\stacktrace,\stdexcept,\stdfloat,\stop_token,\streambuf,\string,\string_view,\strstream,\syncstream,\system_error,\thread,\tuple,\type_traits,\typeindex,\typeinfo,\unordered_map,\unordered_set,\utility,\valarray,\variant,\vector,\xatomic.h,\xatomic_wait.h,\xbit_ops.h,\xcall_once.h,\xcharconv.h,\xcharconv_ryu.h,\xcharconv_ryu_tables.h,\xcharconv_tables.h,\xerrc.h,\xfacet,\xfilesystem_abi.h,\xhash,\xiosbase,\xlocale,\xlocbuf,\xlocinfo,\xlocmes,\xlocmon,\xlocnum,\xloctime,\xmemory,\xnode_handle.h,\xpolymorphic_allocator.h,\xsmf_control.h,\xstring,\xthreads.h,\xtimec.h,\xtr1common,\xtree,\xutility,\ymath.h)";
        uint32_t oldIndex = 0;
        uint32_t index = headerNames.find(',');
        while (index != -1)
        {
            mentioned.emplace(Node::getNodeFromNonNormalizedString(
                includeDir->filePath + string(headerNames.begin() + oldIndex, headerNames.begin() + index), true,
                false));
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
                string str = p.path().string();
                lowerCaseOnWindows(str.data(), str.size());
                // logicalName is a string as it is stored as string_view in reqHeaderNameMapping. reqHeaderNameMapping
                // has string_view so it is fast initialized at build-time.
                logicalName = new string(
                    string{str.data() + includeDir->filePath.size() + 1, str.size() - includeDir->filePath.size() - 1});
                headerNode = Node::getHalfNode(str);

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

            if (useMentioned)
            {
                isHeaderFile = !mentioned.contains(headerNode);
            }

            if (isHeaderFile)
            {
                addHeaderFile(*logicalName, headerNode, true, addInReq, addInUseReq);
            }
            else
            {
                addHeaderUnit(*logicalName, headerNode, true, addInReq, addInUseReq);
            }
        }
    }
}

void CppSourceTarget::actuallyAddInclude(const bool errorOnEmplaceFail, const Node *include, const bool addInReq,
                                         const bool addInUseReq)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (addInReq)
        {
            bool found = false;
            for (const InclNode &inclNode : reqIncls)
            {
                if (inclNode.node == include)
                {
                    if (errorOnEmplaceFail)
                    {
                        printErrorMessage(
                            FORMAT("Include {} is already added in reqIncls in target {}.\n", include->filePath, name));
                    }
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                reqIncls.emplace_back(const_cast<Node *>(include), isSystem, ignoreHeaderDeps);
            }
        }

        if (addInUseReq)
        {
            for (const InclNode &inclNode : useReqIncls)
            {
                if (inclNode.node == include)
                {
                    if (errorOnEmplaceFail)
                    {
                        printErrorMessage(FORMAT("Include {} is already added in useReqIncls in target {}.\n",
                                                 include->filePath, name));
                    }
                    return;
                }
            }

            useReqIncls.emplace_back(const_cast<Node *>(include), isSystem, ignoreHeaderDeps);
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
            writeBigHeaderUnits();
            writeCacheAtConfigTime();
            populateReqAndUseReqDeps();

            for (CppSourceTarget *t : reqDeps)
            {
                for (const auto &[n, t] : t->useReqNodesType)
                {
                    emplaceInNodesType(n, t, true);
                }
            }

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
        else
        {
            populateReqAndUseReqDeps();
        }

        // Needed to maintain ordering between different includes specification.
        reqIncSizeBeforePopulate = reqIncls.size();
        populateTransitiveProperties();

        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            return;
        }

        // getCompileCommand will be later on called concurrently therefore need to set this before.
        setCompileCommand();
        compileCommandWithTool.setCommand(compileCommand);

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

void writeIncDirsAtConfigTime(vector<char> &buffer, const vector<InclNode> &include)
{
    writeUint32(buffer, include.size());
    for (auto &inclNode : include)
    {
        writeNode(buffer, inclNode.node);
    }
}

void readInclDirsAtBuildTime(const char *ptr, uint32_t &bytesRead, vector<InclNode> &include, bool isStandard,
                             bool ignoreHeaderDeps)
{
    const uint32_t reserveSize = readUint32(ptr, bytesRead);
    include.reserve(reserveSize);
    for (uint32_t i = 0; i < reserveSize; ++i)
    {
        include.emplace_back(readHalfNode(ptr, bytesRead), isStandard, ignoreHeaderDeps);
    }
}

void writeHeaderFilesAtConfigTime(vector<char> &buffer, flat_hash_map<string_view, HeaderFileOrUnit> &headerNameMapping,
                                  uint32_t headersSize)
{
    writeUint32(buffer, headersSize);
    for (const auto &[s, h] : headerNameMapping)
    {
        if (h.isUnit)
        {
            continue;
        }

        writeStringView(buffer, s);
        writeNode(buffer, h.data.node);
    }
}

void readHeaderFilesAtBuildTime(const char *ptr, uint32_t &bytesRead,
                                flat_hash_map<string_view, HeaderFileOrUnit> &headerNameMapping, bool isSystem)
{
    const uint32_t includeSize = readUint32(ptr, bytesRead);
    for (uint32_t i = 0; i < includeSize; ++i)
    {
        string_view name = readStringView(ptr, bytesRead);
        Node *node = readHalfNode(ptr, bytesRead);
        if (!headerNameMapping.emplace(name, HeaderFileOrUnit{node, isSystem}).second)
        {
            HMAKE_HMAKE_INTERNAL_ERROR
        }
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

void CppSourceTarget::writeBigHeaderUnits()
{
    auto writeBigHu = [&](const vector<SMFile *> &bigHeaderUnits) {
        for (SMFile *bigHu : bigHeaderUnits)
        {
            if (bigHu)
            {
                string str;
                for (const string &s : bigHu->logicalNames)
                {
                    str += "#include \"" + s + "\"\n";
                }
                string fileStr;
                if (bigHu->node->fileType != file_type::not_found)
                {
                    fileStr = fileToString(bigHu->node->filePath);
                }
                if (fileStr != str)
                {
                    ofstream(bigHu->node->filePath) << str;
                }
                huDeps.emplace_back(bigHu);
            }
        }
    };

    writeBigHu(publicBigHu);
    writeBigHu(privateBigHu);
    writeBigHu(interfaceBigHu);
}

void CppSourceTarget::writeCacheAtConfigTime()
{
    cppBuildCache.deserialize(cacheIndex);
    auto *configBuffer = new vector<char>{};

    writeUint32(*configBuffer, srcFileDeps.size());
    for (SourceNode *source : srcFileDeps)
    {
        string fileNumber = std::to_string(source->node->myId);
        source->objectNode = Node::getNodeFromNormalizedString(
            myBuildDir->filePath + slashc + source->node->getFileName() + fileNumber + ".o", true, true);

        writeNode(*configBuffer, source->node);
        writeNode(*configBuffer, source->objectNode);
    }

    writeUint32(*configBuffer, modFileDeps.size());
    for (SMFile *smFile : modFileDeps)
    {
        string fileNumber = std::to_string(smFile->node->myId);
        smFile->objectNode = Node::getNodeFromNormalizedString(
            myBuildDir->filePath + slashc + smFile->node->getFileName() + fileNumber + ".o", true, true);
        writeNode(*configBuffer, smFile->node);
        writeNode(*configBuffer, smFile->objectNode);
    }

    writeUint32(*configBuffer, imodFileDeps.size());
    for (SMFile *smFile : imodFileDeps)
    {
        string fileNumber = std::to_string(smFile->node->myId);
        smFile->objectNode = Node::getNodeFromNormalizedString(
            myBuildDir->filePath + slashc + smFile->node->getFileName() + fileNumber + ".o", true, true);
        smFile->interfaceNode = Node::getNodeFromNormalizedString(
            myBuildDir->filePath + slashc + smFile->node->getFileName() + fileNumber + ".ifc", true, true);
        writeNode(*configBuffer, smFile->node);
        writeStringView(*configBuffer, smFile->logicalNames[0]);
        writeNode(*configBuffer, smFile->objectNode);
        writeNode(*configBuffer, smFile->interfaceNode);
    }

    writeUint32(*configBuffer, huDeps.size());
    for (SMFile *hu : huDeps)
    {
        string fileNumber = std::to_string(hu->node->myId);
        uint32_t index = findNodeInSourceCache(cppBuildCache.headerUnits, hu->node);
        if (index == -1)
        {
            index = cppBuildCache.headerUnits.size();
            cppBuildCache.headerUnits.emplace_back();
            cppBuildCache.headerUnits[index].srcFile.node = const_cast<Node *>(hu->node);
        }
        hu->indexInBuildCache = index;
        hu->interfaceNode = Node::getNodeFromNormalizedString(
            myBuildDir->filePath + slashc + hu->node->getFileName() + fileNumber + ".ifc", true, true);

        writeUint32(*configBuffer, hu->indexInBuildCache);
        writeNode(*configBuffer, hu->node);
        writeNode(*configBuffer, hu->interfaceNode);

        writeBool(*configBuffer, hu->isReqDep);
        writeBool(*configBuffer, hu->isUseReqDep);

        const uint32_t logicalNamesSize = hu->logicalNames.size();
        writeUint32(*configBuffer, logicalNamesSize);
        for (const string &str : hu->logicalNames)
        {
            writeStringView(*configBuffer, str);
        }

        writeUint32(*configBuffer, hu->composingHeaders.size());
        for (const auto &[headerName, headerNode] : hu->composingHeaders)
        {
            writeStringView(*configBuffer, headerName);
            writeNode(*configBuffer, headerNode);
        }
    }

    writeNode(*configBuffer, myBuildDir);

    if (configuration->evaluate(TreatModuleAsSource::YES))
    {
        writeIncDirsAtConfigTime(*configBuffer, reqIncls);
        writeIncDirsAtConfigTime(*configBuffer, useReqIncls);
    }
    else
    {
        writeHeaderFilesAtConfigTime(*configBuffer, reqHeaderNameMapping, reqHeaderFilesSize);
        writeHeaderFilesAtConfigTime(*configBuffer, useReqHeaderNameMapping, useReqHeaderFilesSize);
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
        smFile->logicalNames.emplace_back(readStringView(ptr, configRead));
        smFile->objectNode = readHalfNode(ptr, configRead);
        smFile->interfaceNode = readHalfNode(ptr, configRead);
        smFile->type =
            smFile->logicalNames[0].contains(':') ? SM_FILE_TYPE::PARTITION_EXPORT : SM_FILE_TYPE::PRIMARY_EXPORT;
        imodNames.emplace(smFile->logicalNames[0], smFile);

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

        hu->isReqDep = readBool(ptr, configRead);
        hu->isUseReqDep = readBool(ptr, configRead);

        const uint32_t logicalNamesSize = readUint32(ptr, configRead);
        hu->logicalNames.reserve(logicalNamesSize);
        for (uint32_t j = 0; j < logicalNamesSize; ++j)
        {
            string_view str = readStringView(ptr, configRead);
            hu->logicalNames.emplace_back(str);
            if (hu->isReqDep)
            {
                reqHeaderNameMapping.emplace(str, HeaderFileOrUnit(hu, isSystem));
            }

            if (hu->isUseReqDep)
            {
                useReqHeaderNameMapping.emplace(str, HeaderFileOrUnit(hu, isSystem));
            }
        }

        const uint32_t headerFileModuleSize = readUint32(ptr, configRead);
        for (uint32_t j = 0; j < headerFileModuleSize; ++j)
        {
            string_view headerFileName = readStringView(ptr, configRead);
            Node *headerNode = readHalfNode(ptr, configRead);
            hu->composingHeaders.emplace(headerFileName, headerNode);
        }

        hu->type = SM_FILE_TYPE::HEADER_UNIT;
        addSelectiveDepNow<0>(*hu);
    }

    myBuildDir = readHalfNode(ptr, configRead);

    if (configuration->evaluate(TreatModuleAsSource::YES))
    {
        readInclDirsAtBuildTime(ptr, configRead, reqIncls, isSystem, ignoreHeaderDeps);
        readInclDirsAtBuildTime(ptr, configRead, useReqIncls, isSystem, ignoreHeaderDeps);
    }
    else
    {
        readHeaderFilesAtBuildTime(ptr, configRead, reqHeaderNameMapping, isSystem);
        readHeaderFilesAtBuildTime(ptr, configRead, useReqHeaderNameMapping, isSystem);
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

BTargetType CppSourceTarget::getBTargetType() const
{
    return BTargetType::CPP_TARGET;
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

void CppSourceTarget::parseRegexSourceDirs(bool assignToSourceNodes, const string &sourceDirectory, string regexStr,
                                           const bool recursive)
{
    if (configuration->evaluate(TreatModuleAsSource::YES))
    {
        assignToSourceNodes = true;
    }

    if constexpr (bsMode == BSMode::BUILD)
    {
        printErrorMessage("Called Wrong time");
    }

    auto addNewFile = [&](const auto &k) {
        if (k.is_regular_file() && regex_match(k.path().filename().string(), std::regex(regexStr)))
        {
            Node *node = Node::getNodeFromNonNormalizedPath(k.path(), true);
            if (assignToSourceNodes)
            {
                actuallyAddSourceFileConfigTime(node);
            }
            else
            {
                actuallyAddModuleFileConfigTime(node, "");
            }
        }
    };

    if (recursive)
    {
        for (const auto &k : recursive_directory_iterator(getNormalizedPath(sourceDirectory)))
        {
            addNewFile(k);
        }
    }
    else
    {
        for (const auto &k : directory_iterator(getNormalizedPath(sourceDirectory)))
        {
            addNewFile(k);
        }
    }
}

void CppSourceTarget::setCompileCommand()
{
    const CompilerFlags &flags = configuration->compilerFlags;
    const Compiler &compiler = configuration->compilerFeatures.compiler;
    compileCommand += '\"';
    compileCommand += configuration->compilerFeatures.compiler.bTPath.string() + "\" ";
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
                str = "/external:I \"";
            }
            else
            {
                str = "/I \"";
            }
        }
        else
        {
            if (isStandard)
            {
                // str += "-isystem ";
                str = "-I \"";
            }
            else
            {
                str = "-I \"";
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

    for (const InclNode &inclNode : reqIncls)
    {
        compileCommand += getIncludeFlag(inclNode.isStandard) + inclNode.node->filePath + "\" ";
    }
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
            std::ranges::transform(define, define.begin(), toupper);
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
        actuallyAddInclude(ptr.reqHuDirs, &ptr, inclNode.node->filePath, inclNode.isSystem,
                           inclNode.ignoreHeaderDeps);
    }
    for (auto &[inclNode, cppSourceTarget] : stored->useReqHuDirs)
    {
        actuallyAddInclude(ptr.useReqHuDirs, &ptr, inclNode.node->filePath, inclNode.isSystem,
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
