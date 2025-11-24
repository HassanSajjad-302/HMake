
#include "CppTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "ConfigurationAssign.hpp"
#include "LOAT.hpp"
#include "rapidhash/rapidhash.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <utility>

using std::filesystem::create_directories, std::filesystem::directory_iterator,
    std::filesystem::recursive_directory_iterator, std::ifstream, std::ofstream, std::regex, std::regex_error;

void CppTarget::readModuleMapFromDir(const string &dir)
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
            printErrorMessage("Error: data found before any mode declaration");
            return;
        }

        if (pendingLogicalName.empty())
        {
            pendingLogicalName = line;
            continue;
        }

        Node *node = Node::getNodeNonNormalized(string(line), true, true);
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

HeaderFileOrUnit::HeaderFileOrUnit(CppMod *cppMod_, const bool isSystem_)
    : data{.cppMod = cppMod_}, isUnit(true), isSystem(isSystem_)
{
}

HeaderFileOrUnit::HeaderFileOrUnit(Node *node_, const bool isSystem_)
    : data{.node = node_}, isUnit(false), isSystem(isSystem_)
{
}

CppTarget::CppTarget(const string &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, false, false), TargetCache(name), configuration(configuration_)
{
    initializeCppTarget(name_, nullptr);
}

CppTarget::CppTarget(const bool buildExplicit, const string &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, buildExplicit, false), TargetCache(name), configuration(configuration_)
{
    initializeCppTarget(name_, nullptr);
}

CppTarget::CppTarget(Node *myBuildDir_, const string &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, false, false), TargetCache(name), configuration(configuration_)
{
    initializeCppTarget(name_, myBuildDir_);
}

CppTarget::CppTarget(Node *myBuildDir_, const bool buildExplicit, const string &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, buildExplicit, false), TargetCache(name), configuration(configuration_)
{
    initializeCppTarget(name_, myBuildDir_);
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

void CppTarget::initializeCppTarget(const string &name_, Node *myBuildDir_)
{
    isSystem = configuration->evaluate(SystemTarget::YES);
    ignoreHeaderDeps = configuration->evaluate(IgnoreHeaderDeps::YES);

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(BigHeaderUnit::YES))
        {
            publicBigHus.emplace_back(nullptr);
            privateBigHus.emplace_back(nullptr);
            interfaceBigHus.emplace_back(nullptr);
        }
        if (!myBuildDir_)
        {
            myBuildDir = Node::getHalfNodeST(configureNode->filePath + slashc + name);
        }
        else
        {
            myBuildDir = myBuildDir_;
        }
        create_directories(myBuildDir->filePath);
    }

    if constexpr (bsMode == BSMode::BUILD)
    {
        // only the first boolean ob hasObjectFiles is read.
        configRead = 0;
        const char *ptr = fileTargetCaches[cacheIndex].configCache.data();
        hasObjectFiles = readBool(ptr, configRead);
        if (configuration->evaluate(IsCppMod::NO))
        {
            readInclDirsAtBuildTime(ptr, configRead, useReqIncls, isSystem, ignoreHeaderDeps);
        }
    }
}

void CppTarget::getObjectFiles(vector<const ObjectFile *> *objectFiles) const
{
    for (const CppMod *objectFile : modFileDeps)
    {
        objectFiles->emplace_back(objectFile);
    }

    for (const CppMod *objectFile : imodFileDeps)
    {
        objectFiles->emplace_back(objectFile);
    }

    for (const CppSrc *objectFile : srcFileDeps)
    {
        objectFiles->emplace_back(objectFile);
    }
}

void CppTarget::populateTransitiveProperties()
{
#ifdef BUILD_MODE

    for (const uint32_t index : reqDepsVecIndices)
    {
        CppTarget *cppTarget = static_cast<CppTarget *>(fileTargetCaches[index].targetCache);
        if (!cppTarget)
        {
            HMAKE_HMAKE_INTERNAL_ERROR
        }
#else
    for (CppTarget *cppTarget : reqDeps)
    {

#endif

        if (configuration->evaluate(IsCppMod::NO))
        {
            for (const InclNode &inclNode : cppTarget->useReqIncls)
            {
                // Configure-time check.
                actuallyAddInclude(true, inclNode.node, true, false);
                reqIncls.emplace_back(inclNode);
            }
        }
        reqCompilerFlags += cppTarget->useReqCompilerFlags;
        for (const Define &define : cppTarget->useReqCompileDefinitions)
        {
            reqCompileDefinitions.emplace(define);
        }
    }
}

void CppTarget::actuallyAddSourceFileConfigTime(Node *node)
{
    if (configuration->evaluate(IsCppMod::YES))
    {
        printErrorMessage(FORMAT("In CppTarget {} source-file {}\n is being added with IsCppMod::YES.\n "
                                 "Please use moduleFiles* API.\n",
                                 name, node->filePath));
    }

    for (const CppSrc *source : srcFileDeps)
    {
        if (source->node == node)
        {
            printErrorMessage(
                FORMAT("Attempting to add {} twice in source-files in cpptarget {}. second insertiion ignored.\n",
                       node->filePath, name));
            return;
        }
    }
    srcFileDeps.emplace_back(new CppSrc(this, node));
}

void CppTarget::actuallyAddModuleFileConfigTime(Node *node, string exportName)
{
    if (configuration->evaluate(IsCppMod::NO))
    {
        printErrorMessage(
            FORMAT("In CppTarget {}\n module-file {}\n is being added with IsCppMod::NO.\n", name, node->filePath));
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
        for (const CppMod *cppMod : modFileDeps)
        {
            if (cppMod->node == node)
            {
                printErrorMessage(
                    FORMAT("Attempting to add {} twice in module-files in cpptarget {}. second insertiion ignored.\n",
                           node->filePath, name));
                return;
            }
        }
        modFileDeps.emplace_back(new CppMod(this, node));
    }
    else
    {
        for (const CppMod *cppMod : imodFileDeps)
        {
            if (cppMod->node == node)
            {
                printErrorMessage(
                    FORMAT("Attempting to add {} twice in module-files in cpptarget {}. second insertiion ignored.\n",
                           node->filePath, name));
                return;
            }
        }
        imodFileDeps.emplace_back(new CppMod(this, node));
        imodFileDeps[imodFileDeps.size() - 1]->logicalNames.emplace_back(exportName);
    }
}

void CppTarget::emplaceInHeaderNameMapping(string_view headerName, HeaderFileOrUnit type, bool addInReq,
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
        string tried =
            type.isUnit ? "Header-Unit " + type.data.cppMod->node->filePath : "Header-File " + type.data.node->filePath;
        string alreadyAdded = it->second.isUnit ? "Header-Unit " + it->second.data.cppMod->node->filePath
                                                : "Header-File " + it->second.data.node->filePath;
        printErrorMessage(
            FORMAT("In CppTarget{}\nFailed adding headerNmae {} in {}headerNameMapping\nTried\n{}\nAlready Added\n{}\n",
                   name, headerName, addInReq ? "req" : "useReq", tried, alreadyAdded));
    }
}

void CppTarget::emplaceInNodesType(const Node *node, FileType type, const bool addInReq)
{
    if (node->filePath.contains("yvals_core.h"))
    {
        bool breakpoint = true;
    }
    if (const auto &[it, ok] = (addInReq ? reqNodesType : useReqNodesType).emplace(node, type);
        !ok && it->second != type)
    {
        printErrorMessage(FORMAT("In CppTarget {}\nnodesTypeMap already has Node {} but with different type\n", name,
                                 node->filePath));
    }
}

void CppTarget::makeHeaderFileAsUnit(const string &includeName, bool addInReq, bool addInUseReq)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        return;
    }

    if (configuration->evaluate(IsCppMod::NO))
    {
        return;
    }

    string *p = new string(includeName);
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

void CppTarget::removeHeaderFile(const string &includeName, const bool addInReq, const bool addInUseReq)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        return;
    }

    string *p = new string(includeName);
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

void CppTarget::removeHeaderUnit(const Node *headerNode, const string &includeName, const bool addInReq,
                                 const bool addInUseReq)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        return;
    }

    string *p = new string(includeName);
    lowerCaseOnWindows(p->data(), p->size());
    bool found = false;
    if (configuration->evaluate(BigHeaderUnit::YES))
    {
        CppMod *bigHu = nullptr;
        if (addInReq && addInUseReq)
        {
            bigHu = publicBigHus[publicBigHus.size() - 1];
        }
        else if (addInReq)
        {
            bigHu = privateBigHus[privateBigHus.size() - 1];
        }
        else if (addInUseReq)
        {
            bigHu = interfaceBigHus[interfaceBigHus.size() - 1];
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
                   headerNode->filePath, includeName, name));
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

void CppTarget::addHeaderFile(const string &includeName, const Node *headerFile, const bool suppressError,
                              const bool addInReq, const bool addInUseReq)
{
    string *p = new string(includeName);
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

void CppTarget::addHeaderUnit(const string &logicalName, const Node *headerUnit, bool suppressError,
                              const bool addInReq, const bool addInUseReq)
{
    if (logicalName.empty())
    {
        bool breakpoint = true;
    }

    string *p = new string(logicalName);
    lowerCaseOnWindows(p->data(), p->size());

    CppMod *hu = nullptr;

    if (configuration->evaluate(BigHeaderUnit::YES))
    {
        if (addInReq && addInUseReq)
        {
            if (name.contains("mp11"))
            {
                bool breakpoint = true;
            }
            const uint32_t index = publicBigHus.size() - 1;
            if (!publicBigHus[index])
            {
                const string str(myBuildDir->filePath + slashc + std::to_string(index) + "public-" +
                                 std::to_string(cacheIndex) + ".hpp");
                const Node *bigHuNode = Node::getNodeNonNormalized(str, true, true);
                publicBigHus[index] = new CppMod(this, bigHuNode);
                emplaceInNodesType(bigHuNode, FileType::HEADER_UNIT, false);
            }
            hu = publicBigHus[index];
        }
        else if (addInReq)
        {
            const uint32_t index = privateBigHus.size() - 1;
            if (!privateBigHus[index])
            {
                const string str(myBuildDir->filePath + slashc + std::to_string(index) + "private-" +
                                 std::to_string(cacheIndex) + ".hpp");
                const Node *bigHuNode = Node::getNodeNonNormalized(str, true, true);
                privateBigHus[index] = new CppMod(this, bigHuNode);
                emplaceInNodesType(bigHuNode, FileType::HEADER_UNIT, false);
            }
            hu = privateBigHus[index];
        }
        else if (addInUseReq)
        {
            const uint32_t index = interfaceBigHus.size() - 1;
            if (!interfaceBigHus[index])
            {
                const string str(myBuildDir->filePath + slashc + std::to_string(index) + "interface-" +
                                 std::to_string(cacheIndex) + ".hpp");
                const Node *bigHuNode = Node::getNodeNonNormalized(str, true, true);
                interfaceBigHus[index] = new CppMod(this, bigHuNode);
                emplaceInNodesType(bigHuNode, FileType::HEADER_UNIT, false);
            }
            hu = interfaceBigHus[index];
        }

        if (addInReq)
        {
            emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{hu, false}, true, suppressError);
            emplaceInNodesType(headerUnit, FileType::HEADER_FILE, true);
            hu->isReqHu = true;
        }

        if (addInUseReq)
        {
            emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{hu, false}, false, suppressError);
            emplaceInNodesType(headerUnit, FileType::HEADER_FILE, false);
            hu->isUseReqHu = true;
        }

        hu->composingHeaders.emplace(*p, const_cast<Node *>(headerUnit));
    }
    else
    {
        for (const CppMod *cppMod : huDeps)
        {
            if (cppMod->node == headerUnit)
            {
                printErrorMessage(FORMAT("Attempting to add {} twice in header-units in cpptarget {}.\n",
                                         headerUnit->filePath, name));
            }
        }

        hu = huDeps.emplace_back(new CppMod(this, headerUnit));

        if (addInReq)
        {
            emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{hu, false}, true, suppressError);
            emplaceInNodesType(headerUnit, FileType::HEADER_UNIT, true);
            hu->isReqHu = true;
        }

        if (addInUseReq)
        {
            emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{hu, false}, false, suppressError);
            emplaceInNodesType(headerUnit, FileType::HEADER_UNIT, false);
            hu->isUseReqHu = true;
        }

        hu->logicalNames.emplace_back(*p);
    }
}

void CppTarget::addHeaderUnitOrFileDir(const Node *includeDir, const string &prefix, const bool isHeaderFile,
                                       const string &regexStr, const bool addInReq, const bool addInUseReq)
{
    if (configuration->evaluate(IsCppMod::NO))
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

void CppTarget::addHeaderUnitOrFileDirMSVC(const Node *includeDir, bool isHeaderFile, const bool useMentioned,
                                           const bool addInReq, const bool addInUseReq, const bool isStandard,
                                           bool ignoreHeaderDeps)
{
    if (configuration->evaluate(IsCppMod::NO))
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
            mentioned.emplace(Node::getNodeNonNormalized(
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

void CppTarget::actuallyAddInclude(const bool errorOnEmplaceFail, const Node *include, const bool addInReq,
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

void CppTarget::updateBTarget(Builder &builder, const unsigned short round, bool &isComplete)
{
    if (!round)
    {
        if (realBTargets[0].updateStatus == UpdateStatus::NEEDS_UPDATE)
        {
            // This is necessary since objectFile->outputFileNode is not updated once after it is compiled.
            realBTargets[0].assignNeedsUpdateToDependents();
        }
    }
    else if (round == 1)
    {
        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            writeBigHeaderUnits();
            populateReqAndUseReqDeps();
            writeCacheAtConfigTime();

            for (CppTarget *t : reqDeps)
            {
                // todo
                // failure error message improvement. should provide complete info and also specify the req cpp-target
                // as well.
                for (const auto &[node, fileType] : t->useReqNodesType)
                {
                    emplaceInNodesType(node, fileType, true);
                }

                for (const auto &p : t->imodNames)
                {
                    if (!imodNames.emplace(p).second)
                    {
                        printErrorMessage(
                            FORMAT("CppTarget {} already has module {}\n", name, p.second->node->filePath));
                    }
                }
                for (const auto &p : t->useReqHeaderNameMapping)
                {
                    emplaceInHeaderNameMapping(p.first, p.second, true, false);
                }

                for (const auto &p : imodNames)
                {
                    if (reqHeaderNameMapping.contains(p.first))
                    {
                        printErrorMessage(
                            FORMAT("CppTarget {} has header-name {} defined as both module and HeaderFileOrUnit\n",
                                   name, p.first));
                    }
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

            for (CppMod *hu : huDeps)
            {
                setHeaderStatusChanged(cppBuildCache.headerUnits[hu->myBuildCacheIndex]);
            }
        }
        else
        {
            readCacheAtBuildTime();
        }

        populateTransitiveProperties();

        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            return;
        }

        // getCompileCommand will be later on called concurrently therefore need to set this before.
        setCompileCommand();
        hashedCompileCommand.setCommand(compileCommand);

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
            if (imodFileDeps[i])
            {
                imodFileDeps[i]->initializeBuildCache(cppBuildCache.imodFiles[i], i);
            }
        }
        for (uint32_t i = 0; i < huDeps.size(); ++i)
        {
            if (huDeps[i])
            {
                huDeps[i]->initializeBuildCache(cppBuildCache.headerUnits[i], i);
            }
        }

        for (const CppTarget *cppTarget : reqDeps)
        {
            if (!cppTarget->modFileDeps.empty())
            {
            }
        }
    }
}

bool CppTarget::writeBuildCache(vector<char> &buffer)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        cppBuildCache.serialize(buffer);
        return true;
    }
    if (atomic_ref(buildCacheUpdated).load(std::memory_order_acquire))
    {
        for (CppSrc *src : srcFileDeps)
        {
            src->updateBuildCache();
        }
        for (CppMod *mod : modFileDeps)
        {
            mod->updateBuildCache();
        }
        for (CppMod *imod : imodFileDeps)
        {
            imod->updateBuildCache();
        }
        for (CppMod *hu : huDeps)
        {
            if (hu)
            {
                hu->updateBuildCache();
            }
        }
        cppBuildCache.serialize(buffer);
        return true;
    }
    return TargetCache::writeBuildCache(buffer);
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

void CppTarget::setHeaderStatusChanged(BuildCache::Cpp::ModuleFile &modCache)
{
    for (Node *node : modCache.srcFile.headerFiles)
    {
        if (auto it = reqNodesType.find(node); it != reqNodesType.end())
        {
            if (it->second != FileType::HEADER_FILE)
            {
                modCache.headerStatusChanged = true;
                return;
            }
        }
        else
        {
            modCache.headerStatusChanged = true;
            return;
        }
    }

    for (BuildCache::Cpp::ModuleFile::SingleHeaderUnitDep &huDep : modCache.headerUnitArray)
    {
        if (auto it = reqNodesType.find(huDep.node); it != reqNodesType.end())
        {
            if (it->second != FileType::HEADER_UNIT)
            {
                modCache.headerStatusChanged = true;
                return;
            }
        }
        else
        {
            modCache.headerStatusChanged = true;
            return;
        }
    }
}

void CppTarget::writeBigHeaderUnits()
{
    if (name.contains("mp11"))
    {
        bool breakpoint = true;
    }
    auto writeBigHu = [&](const vector<CppMod *> &bigHeaderUnits) {
        for (CppMod *bigHu : bigHeaderUnits)
        {
            if (bigHu)
            {
                string str;
                for (const auto &[s, _] : bigHu->composingHeaders)
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

    writeBigHu(publicBigHus);
    writeBigHu(privateBigHus);
    writeBigHu(interfaceBigHus);
}

void CppTarget::writeCacheAtConfigTime()
{
    cppBuildCache.deserialize(cacheIndex);
    auto *configBuffer = new vector<char>{};

    const bool hasObjFiles = !srcFileDeps.empty() || !modFileDeps.empty() || !imodFileDeps.empty();
    writeBool(*configBuffer, hasObjFiles);

    if (configuration->evaluate(IsCppMod::NO))
    {
        writeIncDirsAtConfigTime(*configBuffer, useReqIncls);
    }

    writeUint32(*configBuffer, reqDeps.size());
    for (const CppTarget *r : reqDeps)
    {
        writeUint32(*configBuffer, r->cacheIndex);
    }

    writeUint32(*configBuffer, srcFileDeps.size());
    for (CppSrc *source : srcFileDeps)
    {
        string fileNumber = toString(source->node->myId);
        source->objectNode =
            Node::getNode(myBuildDir->filePath + slashc + source->node->getFileName() + fileNumber + ".o", true, true);

        writeNode(*configBuffer, source->node);
        writeNode(*configBuffer, source->objectNode);
    }

    writeUint32(*configBuffer, modFileDeps.size());
    for (CppMod *cppMod : modFileDeps)
    {
        string fileNumber = toString(cppMod->node->myId);
        cppMod->objectNode =
            Node::getNode(myBuildDir->filePath + slashc + cppMod->node->getFileName() + fileNumber + ".o", true, true);
        writeNode(*configBuffer, cppMod->node);
        writeNode(*configBuffer, cppMod->objectNode);
    }

    writeUint32(*configBuffer, imodFileDeps.size());
    for (CppMod *cppMod : imodFileDeps)
    {
        string fileNumber = toString(cppMod->node->myId);
        uint32_t index = findNodeInSourceCache(cppBuildCache.imodFiles, cppMod->node);
        if (index == -1)
        {
            index = cppBuildCache.imodFiles.size();
            cppBuildCache.imodFiles.emplace_back();
            cppBuildCache.imodFiles[index].srcFile.node = const_cast<Node *>(cppMod->node);
        }
        cppMod->myBuildCacheIndex = index;
        cppMod->objectNode =
            Node::getNode(myBuildDir->filePath + slashc + cppMod->node->getFileName() + fileNumber + ".o", true, true);
        cppMod->interfaceNode = Node::getNode(
            myBuildDir->filePath + slashc + cppMod->node->getFileName() + fileNumber + ".ifc", true, true);
        writeUint32(*configBuffer, cppMod->myBuildCacheIndex);
        writeNode(*configBuffer, cppMod->node);
        writeStringView(*configBuffer, cppMod->logicalNames[0]);
        writeNode(*configBuffer, cppMod->objectNode);
        writeNode(*configBuffer, cppMod->interfaceNode);
    }

    writeUint32(*configBuffer, huDeps.size());
    for (CppMod *hu : huDeps)
    {
        string fileNumber = toString(hu->node->myId);
        uint32_t index = findNodeInSourceCache(cppBuildCache.headerUnits, hu->node);
        if (index == -1)
        {
            index = cppBuildCache.headerUnits.size();
            cppBuildCache.headerUnits.emplace_back();
            cppBuildCache.headerUnits[index].srcFile.node = const_cast<Node *>(hu->node);
        }
        hu->myBuildCacheIndex = index;
        hu->interfaceNode =
            Node::getNode(myBuildDir->filePath + slashc + hu->node->getFileName() + fileNumber + ".ifc", true, true);

        writeUint32(*configBuffer, hu->myBuildCacheIndex);
        writeNode(*configBuffer, hu->node);
        writeNode(*configBuffer, hu->interfaceNode);

        writeBool(*configBuffer, hu->isReqHu);
        writeBool(*configBuffer, hu->isUseReqHu);

        writeUint32(*configBuffer, hu->composingHeaders.size());
        writeUint32(*configBuffer, hu->logicalNames.size());

        for (const auto &[headerName, headerNode] : hu->composingHeaders)
        {
            writeStringView(*configBuffer, headerName);
            writeNode(*configBuffer, headerNode);
        }

        for (const string &str : hu->logicalNames)
        {
            writeStringView(*configBuffer, str);
        }
    }

    writeNode(*configBuffer, myBuildDir);

    if (configuration->evaluate(IsCppMod::NO))
    {
        writeIncDirsAtConfigTime(*configBuffer, reqIncls);
    }
    else
    {
        writeHeaderFilesAtConfigTime(*configBuffer, reqHeaderNameMapping, reqHeaderFilesSize);
        writeHeaderFilesAtConfigTime(*configBuffer, useReqHeaderNameMapping, useReqHeaderFilesSize);
    }

    fileTargetCaches[cacheIndex].configCache = string_view{configBuffer->data(), configBuffer->size()};

    adjustBuildCache(cppBuildCache.srcFiles, srcFileDeps);
    adjustBuildCache(cppBuildCache.modFiles, modFileDeps);
}

void CppTarget::readCacheAtBuildTime()
{
    cppBuildCache.deserialize(cacheIndex);
    const string_view configCache = fileTargetCaches[cacheIndex].configCache;

    const char *ptr = configCache.data();

    const uint32_t reqVecSize = readUint32(ptr, configRead);
    for (uint32_t i = 0; i < reqVecSize; ++i)
    {
        reqDepsVecIndices.emplace_back(readUint32(ptr, configRead));
    }
    const uint32_t sourceSize = readUint32(ptr, configRead);
    srcFileDeps.reserve(sourceSize);
    for (uint32_t i = 0; i < sourceSize; ++i)
    {
        CppSrc *src = srcFileDeps.emplace_back(new CppSrc(this, readHalfNode(ptr, configRead)));
        src->objectNode = readHalfNode(ptr, configRead);

        addDepMT<0>(*srcFileDeps[i]);
    }

    const uint32_t modSize = readUint32(ptr, configRead);
    modFileDeps.reserve(modSize);
    for (uint32_t i = 0; i < modSize; ++i)
    {
        CppMod *cppMod = modFileDeps.emplace_back(new CppMod(this, readHalfNode(ptr, configRead)));
        cppMod->objectNode = readHalfNode(ptr, configRead);
        cppMod->type = SM_FILE_TYPE::PRIMARY_IMPLEMENTATION;

        addDepMT<0>(*cppMod);
    }

    imodFileDeps.resize(cppBuildCache.imodFiles.size());

    const uint32_t imodSize = readUint32(ptr, configRead);
    for (uint32_t i = 0; i < imodSize; ++i)
    {
        const uint32_t indexInBuildCache = readUint32(ptr, configRead);
        CppMod *cppMod = imodFileDeps[indexInBuildCache] = new CppMod(this, readHalfNode(ptr, configRead));
        cppMod->myBuildCacheIndex = indexInBuildCache;

        cppMod->logicalNames.emplace_back(readStringView(ptr, configRead));
        cppMod->objectNode = readHalfNode(ptr, configRead);
        cppMod->interfaceNode = readHalfNode(ptr, configRead);
        cppMod->type =
            cppMod->logicalNames[0].contains(':') ? SM_FILE_TYPE::PARTITION_EXPORT : SM_FILE_TYPE::PRIMARY_EXPORT;
        imodNames.emplace(cppMod->logicalNames[0], cppMod);

        addDepMT<0>(*cppMod);
    }

    huDeps.resize(cppBuildCache.headerUnits.size());

    const uint32_t huSize = readUint32(ptr, configRead);
    for (uint32_t i = 0; i < huSize; ++i)
    {
        const uint32_t indexInBuildCache = readUint32(ptr, configRead);
        CppMod *hu = huDeps[indexInBuildCache] = new CppMod(this, readHalfNode(ptr, configRead));
        hu->myBuildCacheIndex = indexInBuildCache;
        hu->interfaceNode = readHalfNode(ptr, configRead);

        hu->isReqHu = readBool(ptr, configRead);
        hu->isUseReqHu = readBool(ptr, configRead);

        const uint32_t headerFileModuleSize = readUint32(ptr, configRead);
        const uint32_t logicalNamesSize = readUint32(ptr, configRead);
        hu->logicalNames.reserve(logicalNamesSize + headerFileModuleSize);

        for (uint32_t j = 0; j < headerFileModuleSize; ++j)
        {
            string_view headerFileName = readStringView(ptr, configRead);
            Node *headerNode = readHalfNode(ptr, configRead);
            hu->composingHeaders.emplace(headerFileName, headerNode);
            hu->logicalNames.emplace_back(headerFileName);

            if (hu->isReqHu)
            {
                reqHeaderNameMapping.emplace(headerFileName, HeaderFileOrUnit(hu, isSystem));
            }

            if (hu->isUseReqHu)
            {
                useReqHeaderNameMapping.emplace(headerFileName, HeaderFileOrUnit(hu, isSystem));
            }
        }

        for (uint32_t j = 0; j < logicalNamesSize; ++j)
        {
            string_view str = readStringView(ptr, configRead);
            hu->logicalNames.emplace_back(str);
            if (hu->isReqHu)
            {
                reqHeaderNameMapping.emplace(str, HeaderFileOrUnit(hu, isSystem));
            }

            if (hu->isUseReqHu)
            {
                useReqHeaderNameMapping.emplace(str, HeaderFileOrUnit(hu, isSystem));
            }
        }

        hu->type = SM_FILE_TYPE::HEADER_UNIT;
        addDepMT<0, BTargetDepType::SELECTIVE>(*hu);
    }

    myBuildDir = readHalfNode(ptr, configRead);

    if (configuration->evaluate(IsCppMod::NO))
    {
        readInclDirsAtBuildTime(ptr, configRead, reqIncls, isSystem, ignoreHeaderDeps);
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

string CppTarget::getPrintName() const
{
    return "CppTarget " + configureNode->filePath + slashc + name;
}

BTargetType CppTarget::getBTargetType() const
{
    return BTargetType::CPP_TARGET;
}

CppTarget &CppTarget::publicCompilerFlags(const string &compilerFlags)
{
    reqCompilerFlags += compilerFlags;
    useReqCompilerFlags += compilerFlags;
    return *this;
}

CppTarget &CppTarget::privateCompilerFlags(const string &compilerFlags)
{
    reqCompilerFlags += compilerFlags;
    return *this;
}

CppTarget &CppTarget::interfaceCompilerFlags(const string &compilerFlags)
{
    useReqCompilerFlags += compilerFlags;
    return *this;
}

void CppTarget::parseRegexSourceDirs(bool assignToCppSrcs, const string &sourceDirectory, string regexStr,
                                     const bool recursive)
{
    if (configuration->evaluate(IsCppMod::NO))
    {
        assignToCppSrcs = true;
    }

    if constexpr (bsMode == BSMode::BUILD)
    {
        printErrorMessage("Called Wrong time");
    }

    auto addNewFile = [&](const auto &k) {
        if (k.is_regular_file() && regex_match(k.path().filename().string(), std::regex(regexStr)))
        {
            Node *node = Node::getNodeNonNormalized(k.path().string(), true);
            if (assignToCppSrcs)
            {
                actuallyAddSourceFileConfigTime(node);
            }
            else
            {
                actuallyAddModuleFileConfigTime(node, "");
            }
        }
    };

    if (string s = getNormalizedPath(sourceDirectory); !exists(path(s)))
    {
        printErrorMessage(FORMAT("Path {} does not exist in target {}", s, name));
    }

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

void CppTarget::setCompileCommand()
{
    compileCommand.reserve(4 * 1024);
    const CompilerFlags &flags = configuration->compilerFlags;
    const Compiler &compiler = configuration->compilerFeatures.compiler;
    compileCommand += '\"';
    compileCommand += configuration->compilerFeatures.compiler.bTPath + "\" ";
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

string CppTarget::getDependenciesString() const
{
    string deps;
    for (const CppTarget *cppTarget : reqDeps)
    {
        deps += cppTarget->name + '\n';
    }
    return deps;
}

bool operator<(const CppTarget &lhs, const CppTarget &rhs)
{
    return lhs.name < rhs.name;
}

template <> DSC<CppTarget>::DSC(CppTarget *ptr, PLOAT *ploat_, const bool defines, string define_)
{
    objectFileProducer = ptr;
    ploat = ploat_;
    if (ploat_)
    {
        ploat->objectFileProducers.emplace(objectFileProducer);
        if (objectFileProducer->hasObjectFiles)
        {
            ploat->hasObjectFiles = true;
        }

        if (define_.empty())
        {
            define = objectFileProducer->name;
            std::ranges::transform(define, define.begin(), toupper);
            define += "_EXPORT";
        }
        else
        {
            define = std::move(define_);
        }

        // as define is initialized by name if not provided, it might include forward-slash which is not allowed in
        // macro name. so we replace it with underscore instead.
        for (char &c : define)
        {
            if (c == '/')
            {
                c = '_';
            }
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

template <> DSC<CppTarget> &DSC<CppTarget>::save(CppTarget &ptr)
{
    if (!stored)
    {
        stored = static_cast<CppTarget *>(objectFileProducer);
    }
    objectFileProducer = &ptr;
    return *this;
}

template <> DSC<CppTarget> &DSC<CppTarget>::saveAndReplace(CppTarget &ptr)
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

    for (auto &[inclNode, cppTarget] : stored->reqHuDirs)
    {
        actuallyAddInclude(ptr.reqHuDirs, &ptr, inclNode.node->filePath, inclNode.isSystem,
                           inclNode.ignoreHeaderDeps);
    }
    for (auto &[inclNode, cppTarget] : stored->useReqHuDirs)
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

template <> DSC<CppTarget> &DSC<CppTarget>::restore()
{
    objectFileProducer = stored;
    return *this;
}
