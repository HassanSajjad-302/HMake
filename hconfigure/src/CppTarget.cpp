
#include "CppTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "ConfigurationAssign.hpp"
#include "LOAT.hpp"
#include "ToolsCache.hpp"
#include "rapidhash/rapidhash.h"
#include <filesystem>
#include <fstream>
#include <memory_resource>
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

        /*if (currentModeIndex == 0)
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
        }*/
    }
}

HeaderFileOrUnit::HeaderFileOrUnit(const uint32_t targetIndex_, CppMod *cppMod_, bool isSystem_)
    : data{.cppMod = cppMod_}, targetIndex{targetIndex_}, isUnit(true), isSystem(isSystem_)
{
}
HeaderFileOrUnit::HeaderFileOrUnit(const uint32_t targetIndex_, Node *node_, bool isSystem_)
    : data{.node = node_}, targetIndex{targetIndex_}, isUnit(false), isSystem(isSystem_)
{
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
    : ObjectFileProducerWithDS(name_, BTargetType::CPP_TARGET, false, false), configuration(configuration_)
{
    initializeCppTarget(name_, nullptr);
}

CppTarget::CppTarget(const bool buildExplicit, const string &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, BTargetType::CPP_TARGET, buildExplicit, false), configuration(configuration_)
{
    initializeCppTarget(name_, nullptr);
}

CppTarget::CppTarget(Node *myBuildDir_, const string &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, BTargetType::CPP_TARGET, false, false), configuration(configuration_)
{
    initializeCppTarget(name_, myBuildDir_);
}

CppTarget::CppTarget(Node *myBuildDir_, const bool buildExplicit, const string &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, BTargetType::CPP_TARGET, buildExplicit, false), configuration(configuration_)
{
    initializeCppTarget(name_, myBuildDir_);
}

void writeIncDirsAtConfigTime(string &buffer, const vector<InclNode> &include)
{
    writeUint32(buffer, include.size());
    for (auto &inclNode : include)
    {
        writeNode(buffer, inclNode.node);
    }
}

void readInclDirsAtBuildTime(const char *ptr, uint32_t &bytesRead, vector<InclNode> &include, bool isStandard)
{
    const uint32_t reserveSize = readUint32(ptr, bytesRead);
    include.reserve(reserveSize);
    for (uint32_t i = 0; i < reserveSize; ++i)
    {
        include.emplace_back(readHalfNode(ptr, bytesRead), isStandard);
    }
}

void writeHeaderFilesAtConfigTime(string &buffer, const flat_hash_map<string_view, HeaderFileOrUnit> &headerNameMapping)
{
    // Reserve space for the count, fill it in after iteration.
    const uint32_t countOffset = buffer.size();
    writeUint32(buffer, 0);

    uint32_t written = 0;
    for (const auto &[s, h] : headerNameMapping)
    {
        if (h.isUnit)
        {
            continue;
        }
        writeStringView(buffer, s);
        writeNode(buffer, h.data.node);
        ++written;
    }

    memcpy(buffer.data() + countOffset, &written, sizeof(written));
}

void CppTarget::initializeCppTarget(const string &name_, Node *myBuildDir_)
{
    isSystem = configuration->evaluate(SystemTarget::YES);
    useIPC = configuration->evaluate(UseIPC::YES);

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (!myBuildDir_)
        {
            myBuildDir = Node::getHalfNode(configureNode->filePath + slashc + name);
        }
        else
        {
            myBuildDir = myBuildDir_;
        }
        create_directories(myBuildDir->filePath);
    }

    if constexpr (bsMode == BSMode::BUILD)
    {
        readConfigCacheAtBuildTime();
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
    if constexpr (bsMode == BSMode::BUILD)
    {
        for (const uint32_t targetIndex : cachedReqDeps)
        {
            CppTarget *req = static_cast<CppTarget *>(bTargetCaches[targetIndex].bTarget);

            if (configuration->evaluate(IsCppMod::NO) || !useIPC)
            {
                for (const InclNode &inclNode : req->useReqIncls)
                {
                    // Configure-time check.
                    actuallyAddInclude(true, inclNode.node, true, false);
                    reqIncls.emplace_back(inclNode);
                }
            }
            reqCompilerFlags += req->useReqCompilerFlags;
            for (const Define &define : req->useReqCompileDefinitions)
            {
                reqCompileDefinitions.emplace(define);
            }
        }

        return;
    }

    for (CppTarget *cppTarget : reqDeps)
    {
        if (configuration->evaluate(IsCppMod::NO) || !useIPC)
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

void CppTarget::actuallyAddSourceFileConfigTime(const Node *node)
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
    srcFileDeps.emplace_back(new CppSrc(this, node, CppModType::CPP_SRC));
}

string CppTarget::getExportNameFromFirstLine(const Node *node)
{
    constexpr string_view kModuleTag = "// module:";

    ifstream file(node->filePath);
    if (!file)
    {
        printErrorMessage(
            FORMAT("Could not read the first line of file\n{}\n in actuallyAddModuleFileConfigTime\n", node->filePath));
        return {};
    }

    string firstLine;
    if (!std::getline(file, firstLine))
    {
        printErrorMessage(
            FORMAT("Could not read the first line of file\n{}\n in actuallyAddModuleFileConfigTime\n", node->filePath));
        return {};
    }

    if (!firstLine.empty() && firstLine.back() == '\r')
    {
        firstLine.pop_back();
    }

    if (!firstLine.starts_with(kModuleTag))
    {
        return {};
    }

    string_view exportName(firstLine.data() + kModuleTag.size(), firstLine.size() - kModuleTag.size());
    while (!exportName.empty() && static_cast<unsigned char>(exportName.front()) <= ' ')
    {
        exportName.remove_prefix(1);
    }
    while (!exportName.empty() && static_cast<unsigned char>(exportName.back()) <= ' ')
    {
        exportName.remove_suffix(1);
    }

    return string{exportName};
}

void CppTarget::actuallyAddModuleFileConfigTime(const Node *node, string exportName)
{
    if (configuration->evaluate(IsCppMod::NO))
    {
        printErrorMessage(
            FORMAT("In CppTarget {}\n module-file {}\n is being added with IsCppMod::NO.\n", name, node->filePath));
    }

    if (exportName.empty())
    {
        string fileName = node->getFileName();
        if (const string ext = node->getExtension(); ext == ".cppm" || ext == ".ixx")
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
        modFileDeps.emplace_back(new CppMod(this, node, CppModType::PRIMARY_IMPLEMENTATION));
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
        imodFileDeps.emplace_back(new CppMod(
            this, node, exportName.contains(':') ? CppModType::PARTITION_EXPORT : CppModType::PRIMARY_EXPORT));
        imodFileDeps[imodFileDeps.size() - 1]->logicalNames.emplace_back(exportName);
    }
}

void CppTarget::checkSameHeaderNameMapping(const string_view headerName)
{
    const auto &it = reqHeaderNameMapping.find(headerName);
    const auto &it2 = useReqHeaderNameMapping.find(headerName);
    if (it != reqHeaderNameMapping.end() && it2 != useReqHeaderNameMapping.end())
    {
        if (it->second.data.node != it2->second.data.node)
        {
            string str;
            if (it->second.isUnit)
            {
                str = FORMAT(
                    "In CppTarget {}\nheader-name{}\n is mapped to different Header-Units in reqHeaderMapping and "
                    "useReqHeaderMapping respectively\n{}\n{}\n",
                    name, it->first, it->second.data.cppMod->node->filePath, it2->second.data.cppMod->node->filePath);
            }
            else
            {
                str = FORMAT(
                    "In CppTarget {}\nheader-name{}\n is mapped to different Header-Files in reqHeaderMapping and "
                    "useReqHeaderMapping respectively\n{}\n{}\n",
                    name, it->first, it->second.data.node->filePath, it->second.data.node->filePath);
            }
            printErrorMessage(str);
        }
    }
}

void CppTarget::populateNameMappingsAndNodesType()
{
    if (configuration->evaluate(UseConfigurationScope::YES))
    {
        flat_hash_map<string_view, HeaderFileOrUnit> tempNameMapping;
        tempNameMapping.reserve(reqHeaderNameMapping.size() + useReqHeaderNameMapping.size());
        tempNameMapping.insert(reqHeaderNameMapping.begin(), reqHeaderNameMapping.end());
        tempNameMapping.insert(useReqHeaderNameMapping.begin(), useReqHeaderNameMapping.end());

        for (const auto &[headerName, type] : tempNameMapping)
        {
            if (const auto &[it, ok] = configuration->headerNameMapping.emplace(headerName, vector(1, type)); !ok)
            {
                string alreadyAdded;
                string tried;

                const HeaderFileOrUnit headerFileOrUnit = it->second[0];
                if (type.isUnit)
                {
                    tried = "Header-Unit " + type.data.cppMod->node->filePath;
                    alreadyAdded = "Header-Unit " + headerFileOrUnit.data.cppMod->node->filePath;
                }
                else
                {
                    tried = "Header-File " + type.data.node->filePath;
                    alreadyAdded = "Header-File " + headerFileOrUnit.data.node->filePath;
                }

                printErrorMessage(FORMAT("In Configuration {}\nFailed adding headerName {} in "
                                         "headerNameMapping\nTried\n{}\nAlready Added\n{}\n",
                                         configuration->name, headerName, tried, alreadyAdded));
            }
        }

        flat_hash_map<const Node *, FileType> tempNodesType;
        tempNodesType.merge(reqNodesType);
        tempNodesType.merge(useReqNodesType);

        for (const auto &[node, type] : tempNodesType)
        {
            if (const auto &[it, ok] = configuration->nodesType.emplace(node, type); !ok)
            {
                printErrorMessage(FORMAT("Configuration {}\nnodesTypeMap already has Node {} with type\n",
                                         configuration->name, node->filePath));
            }
        }
    }

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
                printErrorMessage(FORMAT("CppTarget {} already has module {}\n", name, p.second->node->filePath));
            }
        }
        for (const auto &p : t->useReqHeaderNameMapping)
        {
            emplaceInHeaderNameMapping(p.first, p.second, true);
        }

        for (const auto &p : imodNames)
        {
            if (reqHeaderNameMapping.contains(p.first))
            {
                printErrorMessage(FORMAT(
                    "CppTarget {} has header-name {} defined as both module and HeaderFileOrUnit\n", name, p.first));
            }
        }
    }
}

void CppTarget::emplaceInHeaderNameMapping(string_view headerName, HeaderFileOrUnit type, const bool addInReq)
{
    const auto &[it, ok] = (addInReq ? reqHeaderNameMapping : useReqHeaderNameMapping).emplace(headerName, type);
    const HeaderFileOrUnit headerFileOrUnit = it->second;

    if (ok)
    {
        return;
    }

    string alreadyAdded;
    string tried;
    if (type.isUnit)
    {
        tried = "Header-Unit " + type.data.cppMod->node->filePath;
        alreadyAdded = "Header-Unit " + headerFileOrUnit.data.cppMod->node->filePath;
    }
    else
    {
        tried = "Header-File " + type.data.node->filePath;
        alreadyAdded = "Header-File " + headerFileOrUnit.data.node->filePath;
    }

    printErrorMessage(
        FORMAT("In CppTarget {}\nFailed adding headerName {} in {}headerNameMapping\nTried\n{}\nAlready Added\n{}\n",
               name, headerName, addInReq ? "req" : "useReq", tried, alreadyAdded));
}

void CppTarget::emplaceInNodesType(const Node *node, FileType type, const bool addInReq)
{
    if (const auto &[it, ok] = (addInReq ? reqNodesType : useReqNodesType).emplace(node, type); !ok)
    {
        printErrorMessage(
            FORMAT("In CppTarget {}\nnodesTypeMap already has Node {} with type\n", name, node->filePath));
    }
}

const Node *CppTarget::getIncludeNode(const bool isHeaderFile, const string &includeName, const bool addInReq,
                                      const bool addInUseReq)
{
    if (isHeaderFile)
    {
        if (addInReq)
        {
            if (const auto it = reqHeaderNameMapping.find(includeName); it != reqHeaderNameMapping.end())
            {
                return it->second.data.node;
            }
        }
        else
        {
            if (const auto it = useReqHeaderNameMapping.find(includeName); it != useReqHeaderNameMapping.end())
            {
                return it->second.data.node;
            }
        }
        return nullptr;
    }

    if (configuration->evaluate(BigHeaderUnit::YES))
    {
        CppMod *hu = nullptr;
        if (addInReq && addInUseReq)
        {
            hu = getPublicBigHu(false);
        }
        else if (addInReq)
        {
            hu = getPrivateBigHu(false);
        }
        else if (addInUseReq)
        {
            hu = getInterfaceBigHu(false);
        }

        if (hu)
        {
            if (const auto it = hu->composingHeaders.find(includeName); it != hu->composingHeaders.end())
            {
                return it->second;
            }
        }
        return nullptr;
    }

    if (addInReq)
    {
        if (const auto it = reqHeaderNameMapping.find(includeName); it != reqHeaderNameMapping.end())
        {
            return it->second.data.cppMod->node;
        }
    }
    else
    {
        if (const auto it = useReqHeaderNameMapping.find(includeName); it != useReqHeaderNameMapping.end())
        {
            return it->second.data.cppMod->node;
        }
    }

    return nullptr;
}

void CppTarget::makeHeaderFileHeaderUnit(const string &includeName, bool addInReq, bool addInUseReq)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        return;
    }

    if (configuration->isCppMod == IsCppMod::NO)
    {
        return;
    }

    string *p = new string(includeName);
    lowerCaseOnWindows(p->data(), p->size());
    const Node *headerNode = getIncludeNode(true, *p, addInReq, addInUseReq);
    if (!headerNode)
    {
        printErrorMessage(FORMAT("Include-Name {} does not exist in target {} while calling makeHeaderFileHeaderUnit\n",
                                 includeName, name));
    }

    removeHeaderFile(includeName, addInReq, addInUseReq);
    addHeaderUnit(includeName, headerNode, addInReq, addInUseReq);
}

void CppTarget::makeHeaderUnitHeaderFile(const string &includeName, bool addInReq, bool addInUseReq)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        return;
    }

    if (configuration->isCppMod == IsCppMod::NO)
    {
        return;
    }

    string *p = new string(includeName);
    lowerCaseOnWindows(p->data(), p->size());
    const Node *headerNode = getIncludeNode(false, *p, addInReq, addInUseReq);
    if (!headerNode)
    {
        printErrorMessage(FORMAT("Include-Name {} does not exist in target {} while calling makeHeaderUnitHeaderFile\n",
                                 includeName, name));
    }

    removeHeaderUnit(includeName, addInReq, addInUseReq);
    addHeaderFile(includeName, headerNode, addInReq, addInUseReq);
}

void CppTarget::removeHeaderFile(const string &includeName, const bool addInReq, const bool addInUseReq)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        return;
    }

    string *p = new string(includeName);
    lowerCaseOnWindows(p->data(), p->size());
    const Node *headerNode = getIncludeNode(true, *p, addInReq, addInUseReq);

    if (addInReq)
    {
        if (const auto &it = reqHeaderNameMapping.find(*p); it == reqHeaderNameMapping.end())
        {
            printErrorMessage(FORMAT("Could not find the header {}\n in removeHeaderFile in target {}\n", *p, name));
        }
        else
        {
            reqHeaderNameMapping.erase(it);
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
            useReqHeaderNameMapping.erase(it);
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

void CppTarget::removeHeaderUnit(const string &includeName, const bool addInReq, const bool addInUseReq)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        return;
    }

    string *p = new string(includeName);
    lowerCaseOnWindows(p->data(), p->size());
    const Node *headerNode = getIncludeNode(false, *p, addInReq, addInUseReq);

    if (configuration->evaluate(BigHeaderUnit::YES))
    {
        // todo: following incomplete. not reviewed.
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

        if (addInReq)
        {
            if (const auto &it = reqHeaderNameMapping.find(*p); it == reqHeaderNameMapping.end())
            {
                printErrorMessage(
                    FORMAT("Could not find the header {}\n in removeHeaderUnit in target {}\n", *p, name));
            }
            else
            {
                reqHeaderNameMapping.erase(it);
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
                printErrorMessage(
                    FORMAT("Could not find the header {}\n in removeHeaderUnit in target {}\n", *p, name));
            }
            else
            {
                useReqHeaderNameMapping.erase(it);
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

        if (!bigHu->composingHeaders.erase(*p))
        {
            printErrorMessage(
                FORMAT("Could not find composing-header {} to remove in public-header-unit in target {}\n.", *p, name));
        }
        if (bigHu->composingHeaders.empty())
        {
            delete bigHu;
        }
        return;
    }

    bool found = false;
    for (auto it = huDeps.begin(); it != huDeps.end(); ++it)
    {
        if ((*it)->node == headerNode)
        {
            huDeps.erase(it);
            found = true;
            break;
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
        if (const auto &it = reqHeaderNameMapping.find(*p); it == reqHeaderNameMapping.end())
        {
            printErrorMessage(FORMAT("Could not find the header {}\n in removeHeaderUnit in target {}\n", *p, name));
        }
        else
        {
            reqHeaderNameMapping.erase(it);
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
            printErrorMessage(FORMAT("Could not find the header {}\n in removeHeaderUnit in target {}\n", *p, name));
        }
        else
        {
            useReqHeaderNameMapping.erase(it);
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

void CppTarget::addHeaderFile(const string &includeName, const Node *headerFile, const bool addInReq,
                              const bool addInUseReq)
{
    string *p = new string(includeName);
    lowerCaseOnWindows(p->data(), p->size());
    if (addInReq)
    {
        emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{const_cast<Node *>(headerFile), isSystem}, true);
        emplaceInNodesType(headerFile, FileType::HEADER_FILE, true);
    }

    if (addInUseReq)
    {
        emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{const_cast<Node *>(headerFile), isSystem}, false);
        emplaceInNodesType(headerFile, FileType::HEADER_FILE, false);
    }

    checkSameHeaderNameMapping(*p);
}

void CppTarget::addHeaderUnit(const string &includeName, const Node *headerUnit, const bool addInReq,
                              const bool addInUseReq)
{
    if (configuration->evaluate(TreatHUAsHeaderFile::YES))
    {
        addHeaderFile(includeName, headerUnit, addInReq, addInUseReq);
        return;
    }

    string *p = new string(includeName);
    lowerCaseOnWindows(p->data(), p->size());

    CppMod *hu = nullptr;

    if (configuration->evaluate(BigHeaderUnit::YES))
    {
        if (addInReq && addInUseReq)
        {
            hu = getPublicBigHu(false);
        }
        else if (addInReq)
        {
            hu = getPrivateBigHu(false);
        }
        else if (addInUseReq)
        {
            hu = getInterfaceBigHu(false);
        }

        if (addInReq)
        {
            emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{hu, false}, true);
            emplaceInNodesType(headerUnit, FileType::HEADER_FILE, true);
        }

        if (addInUseReq)
        {
            emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{hu, false}, false);
            emplaceInNodesType(headerUnit, FileType::HEADER_FILE, false);
        }

        checkSameHeaderNameMapping(*p);
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

        hu = huDeps.emplace_back(new CppMod(this, headerUnit, CppModType::HEADER_UNIT));

        if (addInReq)
        {
            emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{hu, false}, true);
            emplaceInNodesType(headerUnit, FileType::HEADER_UNIT, true);
            hu->isReqHu = true;
        }

        if (addInUseReq)
        {
            emplaceInHeaderNameMapping(*p, HeaderFileOrUnit{hu, false}, false);
            emplaceInNodesType(headerUnit, FileType::HEADER_UNIT, false);
            hu->isUseReqHu = true;
        }

        checkSameHeaderNameMapping(*p);
        hu->logicalNames.emplace_back(*p);
    }
}

void CppTarget::addHeaderUnitOrFileDir(const Node *includeDir, const string &prefix, const bool isHeaderFile,
                                       const string &regexStr, const bool addInReq, const bool addInUseReq)
{
    // if we are going to build hu or module conventionally, then we will not store the logical-name to file mapping in
    // the cache.
    if (configuration->evaluate(IsCppMod::NO))
    {
        return;
    }

    for (const auto &p : directory_iterator(includeDir->filePath))
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
                addHeaderFile(*logicalName, headerNode, addInReq, addInUseReq);
            }
            else
            {
                addHeaderUnit(*logicalName, headerNode, addInReq, addInUseReq);
            }
        }
    }
}

CppMod *CppTarget::getPublicBigHu(const bool addNew)
{
    if (addNew || publicBigHus.empty())
    {
        const uint32_t index = publicBigHus.size();
        publicBigHus.emplace_back(nullptr);
        const string str(myBuildDir->filePath + slashc + std::to_string(index) + "public-" +
                         std::to_string(cacheIndex) + ".hpp");
        const Node *bigHuNode = Node::getNodeNonNormalized(str, true, true);
        publicBigHus[index] = new CppMod(this, bigHuNode, CppModType::HEADER_UNIT);
        publicBigHus[index]->isReqHu = true;
        publicBigHus[index]->isUseReqHu = true;
        emplaceInNodesType(bigHuNode, FileType::HEADER_UNIT, false);
        return publicBigHus[index];
    }
    return publicBigHus[publicBigHus.size() - 1];
}

CppMod *CppTarget::getPrivateBigHu(const bool addNew)
{
    if (addNew || privateBigHus.empty())
    {
        const uint32_t index = privateBigHus.size();
        privateBigHus.emplace_back(nullptr);
        const string str(myBuildDir->filePath + slashc + std::to_string(index) + "private-" +
                         std::to_string(cacheIndex) + ".hpp");
        const Node *bigHuNode = Node::getNodeNonNormalized(str, true, true);
        privateBigHus[index] = new CppMod(this, bigHuNode, CppModType::HEADER_UNIT);
        privateBigHus[index]->isReqHu = true;
        emplaceInNodesType(bigHuNode, FileType::HEADER_UNIT, false);
    }
    return privateBigHus[privateBigHus.size() - 1];
}

CppMod *CppTarget::getInterfaceBigHu(const bool addNew)
{
    if (addNew || interfaceBigHus.empty())
    {
        const uint32_t index = interfaceBigHus.size();
        interfaceBigHus.emplace_back(nullptr);
        const string str(myBuildDir->filePath + slashc + std::to_string(index) + "interface-" +
                         std::to_string(cacheIndex) + ".hpp");
        const Node *bigHuNode = Node::getNodeNonNormalized(str, true, true);
        interfaceBigHus[index] = new CppMod(this, bigHuNode, CppModType::HEADER_UNIT);
        interfaceBigHus[index]->isUseReqHu = true;
        emplaceInNodesType(bigHuNode, FileType::HEADER_UNIT, false);
        return interfaceBigHus[index];
    }
    return interfaceBigHus[interfaceBigHus.size() - 1];
}

void CppTarget::parseAndAddInComposingHeaders(CppMod &hu, const string &headerNames)
{

    uint32_t oldIndex = 0;
    uint32_t index = headerNames.find(',');
    while (index != -1)
    {
        // logicalName is a string as it is stored as string_view in reqHeaderNameMapping. reqHeaderNameMapping
        // has string_view so it is fast initialized at build-time.
        string *logicalName = new string(headerNames.begin() + oldIndex, headerNames.begin() + index);

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

        if (!hu.composingHeaders.emplace(*logicalName, nullptr).second)
        {
            printErrorMessage(
                FORMAT("Emplace of logicalName {}\n failed for hu {}\n", *logicalName, hu.node->filePath));
        }

        oldIndex = index + 1;
        index = headerNames.find(',', oldIndex);
    }
}

void CppTarget::addComposingHeadersMSVC()
{
    if (configuration->evaluate(IsCppMod::NO))
    {
        return;
    }

    // From the header-units.json in the include-dir, the mentioned header-files are manually added as parsing of
    // header-units.json file fails because of the comments in it.
    string headerNames =
        R"(__msvc_bit_utils.hpp,__msvc_chrono.hpp,__msvc_cxx_stdatomic.hpp,__msvc_filebuf.hpp,__msvc_format_ucd_tables.hpp,__msvc_formatter.hpp,__msvc_heap_algorithms.hpp,__msvc_int128.hpp,__msvc_iter_core.hpp,__msvc_minmax.hpp,__msvc_ostream.hpp,__msvc_print.hpp,__msvc_ranges_to.hpp,__msvc_ranges_tuple_formatter.hpp,__msvc_sanitizer_annotate_container.hpp,__msvc_string_view.hpp,__msvc_system_error_abi.hpp,__msvc_threads_core.hpp,__msvc_tzdb.hpp,__msvc_xlocinfo_types.hpp,algorithm,any,array,atomic,barrier,bit,bitset,cassert,cctype,cerrno,cfenv,cfloat,charconv,chrono,cinttypes,climits,clocale,cmath,codecvt,compare,complex,concepts,condition_variable,coroutine,csetjmp,csignal,cstdarg,cstddef,cstdint,cstdio,cstdlib,cstring,ctime,cuchar,cwchar,cwctype,deque,exception,execution,expected,filesystem,format,forward_list,fstream,functional,future,generator,initializer_list,iomanip,ios,iosfwd,iostream,iso646.h,istream,iterator,latch,limits,list,locale,map,mdspan,memory,memory_resource,mutex,new,numbers,numeric,optional,ostream,print,queue,random,ranges,ratio,regex,scoped_allocator,semaphore,set,shared_mutex,source_location,span,spanstream,sstream,stack,stacktrace,stdexcept,stdfloat,stop_token,streambuf,string,string_view,strstream,syncstream,system_error,thread,tuple,type_traits,typeindex,typeinfo,unordered_map,unordered_set,utility,valarray,variant,vector,xatomic.h,xatomic_wait.h,xbit_ops.h,xcall_once.h,xcharconv.h,xcharconv_ryu.h,xcharconv_ryu_tables.h,xcharconv_tables.h,xerrc.h,xfacet,xfilesystem_abi.h,xhash,xiosbase,xlocale,xlocbuf,xlocinfo,xlocmes,xlocmon,xlocnum,xloctime,xmemory,xnode_handle.h,xpolymorphic_allocator.h,xsmf_control.h,xstring,xthreads.h,xtimec.h,xtr1common,xtree,xutility,ymath.h,)";

    // C compatibility headers
    headerNames +=
        R"(assert.h,ctype.h,errno.h,fenv.h,float.h,inttypes.h,limits.h,locale.h,math.h,setjmp.h,signal.h,stdarg.h,stddef.h,stdint.h,stdio.h,stdlib.h,string.h,time.h,uchar.h,wchar.h,wctype.h,)";

    // C++ version
    /*headerNames +=
        R"(cassert,cctype,cerrno,cfenv,cfloat,cinttypes,climits,clocale,cmath,csetjmp,csignal,cstdarg,cstddef,cstdint,cstdio,cstdlib,cstring,ctime,cuchar,cwchar,cwctype,)";*/

    // intrinsicsl
    headerNames += "intrin.h,";

    // needed by boost
    headerNames += "crtdbg.h,ntverp.h,version,";

    parseAndAddInComposingHeaders(*getPublicBigHu(true), headerNames);
    headerNames = "windows.h,winapifamily.h,";
    parseAndAddInComposingHeaders(*getPublicBigHu(true), headerNames);
}

void CppTarget::addComposingHeadersLinux()
{
    if (configuration->evaluate(IsCppMod::NO))
    {
        return;
    }

    string headerNames;

    // C compatibility headers
    headerNames +=
        R"(assert.h,ctype.h,errno.h,fenv.h,float.h,inttypes.h,limits.h,locale.h,math.h,setjmp.h,signal.h,stdarg.h,stddef.h,stdint.h,stdio.h,stdlib.h,string.h,time.h,uchar.h,wchar.h,wctype.h,stdbool.h,)";

    // First header-unit of C compatibility headers
    // parseAndAddInComposingHeaders(*getPublicBigHu(true), headerNames);

    // C++ headers
    headerNames +=
        R"(algorithm,any,array,atomic,barrier,bit,bitset,charconv,chrono,compare,complex,concepts,condition_variable,coroutine,deque,exception,execution,expected,filesystem,format,forward_list,fstream,functional,future,generator,initializer_list,iomanip,ios,iosfwd,iostream,istream,iterator,latch,limits,list,locale,map,memory,memory_resource,mutex,new,numbers,numeric,optional,ostream,print,queue,random,ranges,ratio,regex,scoped_allocator,semaphore,set,shared_mutex,source_location,span,spanstream,sstream,stack,stacktrace,stdexcept,stop_token,streambuf,string,string_view,syncstream,system_error,text_encoding,thread,tuple,type_traits,typeindex,typeinfo,unordered_map,unordered_set,utility,valarray,variant,vector,version,)";

    // Compiler Specific Header ( GCC and Clang).
    headerNames += R"(cxxabi.h,execinfo.h,unwind.h,immintrin.h,emmintrin.h,)";

    // C++ version of C compatibility headers
    headerNames +=
        R"(cassert,cctype,cerrno,cfenv,cfloat,cinttypes,climits,clocale,cmath,csetjmp,csignal,cstdarg,cstddef,cstdint,cstdio,cstdlib,cstring,ctime,cuchar,cwchar,cwctype,)";

    // Second header-unit of C++ headers
    // parseAndAddInComposingHeaders(*getPublicBigHu(true), headerNames);

    // System Posix
    headerNames +=
        R"(dirent.h,dlfcn.h,endian.h,fcntl.h,malloc.h,poll.h,pthread.h,pwd.h,sched.h,spawn.h,sysexits.h,unistd.h,sys/ioctl.h,sys/mman.h,sys/resource.h,sys/socket.h,sys/stat.h,sys/syscall.h,sys/time.h,sys/types.h,sys/un.h,sys/vfs.h,sys/wait.h,sys/utsname.h,cpuid.h,)";

    // Third header-unit of Posix headers
    parseAndAddInComposingHeaders(*getPublicBigHu(true), headerNames);

    // The following 3 are separate as macros from elf.h included directly or trasnitively in the first 2 collide with
    // ELF.cpp. Similarly, macros from the last collide with COFFImportFile.cpp
    headerNames = "link.h,sys/auxv.h,zlib.h,";

    // A fourth one to compile ELF.cpp as macros collide with elf.h.
    // Similarly
    parseAndAddInComposingHeaders(*getPublicBigHu(true), headerNames);

    /*
    headerNames = "zlib.h,";
    // A fifth one to compile COFFImportFile.cpp as macros collide with zconf.h
    parseAndAddInComposingHeaders(*getPublicBigHu(true), headerNames);
*/
}

void CppTarget::addComposingHeadersDir(const Node *includeDir)
{
    if (configuration->evaluate(IsCppMod::NO))
    {
        return;
    }

    CppMod *publicBigHu = getPublicBigHu(false);
    for (const auto &f : directory_iterator(includeDir->filePath))
    {
        if (f.is_regular_file())
        {
            string *logicalName;
            {
                string str = f.path().string();
                lowerCaseOnWindows(str.data(), str.size());
                // logicalName is a string as it is stored as string_view in reqHeaderNameMapping. reqHeaderNameMapping
                // has string_view so it is fast initialized at build-time.
                logicalName = new string(str.data() + includeDir->filePath.size() + 1,
                                         str.size() - includeDir->filePath.size() - 1);

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
            publicBigHu->composingHeaders.emplace(*logicalName, nullptr);
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
                reqIncls.emplace_back(const_cast<Node *>(include), isSystem);
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

            useReqIncls.emplace_back(const_cast<Node *>(include), isSystem);
        }
    }
}

void CppTarget::setCommandHashes()
{
    uint64_t cppHash = 0, cHash = 0, assemblyHash = 0;
    bool cppDone = false, cDone = false, assemblyDone = false;

    auto hashCommand = [&](const string &baseCommand, uint64_t &hash, bool &done) -> uint64_t {
        if (!done)
        {
            STACK_PMR_STRING(cmd, 64 * 1024);
            cmd = baseCommand;
            setCompileCommand(cmd);
            hash = rapidhash(cmd.data(), cmd.size());
            done = true;
        }
        return hash;
    };

    auto getHash = [&](const SourceType sourceType) -> uint64_t {
        if (sourceType == SourceType::CPP)
        {
            return hashCommand(configuration->cppCompileCommand, cppHash, cppDone);
        }
        if (sourceType == SourceType::C)
        {
            return hashCommand(configuration->cCompileCommand, cHash, cDone);
        }
        return hashCommand(configuration->assemblyCompileCommand, assemblyHash, assemblyDone);
    };

    auto sourceTypeOf = [](const std::string_view path) -> SourceType {
        if (path.ends_with(".c"))
        {
            return SourceType::C;
        }
        if (path.ends_with(".S") || path.ends_with(".s"))
        {
            return SourceType::ASSEMBLY;
        }
        return SourceType::CPP;
    };

    for (CppSrc *srcFileDep : srcFileDeps)
    {
        srcFileDep->sourceType = sourceTypeOf(srcFileDep->node->filePath);
        srcFileDep->commandHash = getHash(srcFileDep->sourceType);
    }
    for (CppMod *modFileDep : modFileDeps)
    {
        modFileDep->sourceType = sourceTypeOf(modFileDep->node->filePath);
        modFileDep->commandHash = getHash(modFileDep->sourceType);
    }
    for (CppMod *imodFileDep : imodFileDeps)
    {
        imodFileDep->sourceType = SourceType::CPP;
        imodFileDep->commandHash = getHash(SourceType::CPP);
    }
    for (CppMod *huDep : huDeps)
    {
        huDep->sourceType = SourceType::CPP;
        huDep->commandHash = getHash(SourceType::CPP);
    }
}

void CppTarget::completeRoundOne()
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        populateTransitiveProperties();
        setCommandHashes();
        return;
    }

    writeBigHeaderUnits();
    populateReqAndUseReqDeps();
    populateNameMappingsAndNodesType();
    if (configuration->evaluate(UseConfigurationScope::NO))
    {
        setHeaderFileStatusChanged(false);
    }
    populateTransitiveProperties();
    setCommandHashes();
    return;
}

void CppTarget::writeConfigCacheAtConfigTime(string &buffer)
{
    writeUint32(buffer, reqDeps.size());
    for (const CppTarget *r : reqDeps)
    {
        writeUint32(buffer, r->cacheIndex);
    }

    const bool hasObjFiles = !srcFileDeps.empty() || !modFileDeps.empty() || !imodFileDeps.empty();
    writeBool(buffer, hasObjFiles);

    writeUint32(buffer, srcFileDeps.size());
    for (const CppSrc *source : srcFileDeps)
    {
        writeNode(buffer, source->node);
    }

    writeUint32(buffer, modFileDeps.size());
    for (const CppMod *cppMod : modFileDeps)
    {
        writeNode(buffer, cppMod->node);
    }

    writeUint32(buffer, imodFileDeps.size());
    for (const CppMod *cppMod : imodFileDeps)
    {
        writeNode(buffer, cppMod->node);
        writeBool(buffer, cppMod->type == CppModType::PRIMARY_EXPORT);
    }

    writeUint32(buffer, huDeps.size());
    for (const CppMod *hu : huDeps)
    {
        writeNode(buffer, hu->node);
    }

    writeNode(buffer, myBuildDir);

    if (configuration->evaluate(IsCppMod::NO) || !useIPC)
    {
        writeIncDirsAtConfigTime(buffer, reqIncls);
        writeIncDirsAtConfigTime(buffer, useReqIncls);
    }

    if (configuration->evaluate(IsCppMod::YES))
    {
        writeHeaderFilesAtConfigTime(buffer, reqHeaderNameMapping);
        writeHeaderFilesAtConfigTime(buffer, useReqHeaderNameMapping);
    }
}

void CppTarget::setHeaderFileStatusChangedCppMod(const vector<CppMod *> &cppModVec, const bool calledFromConfiguration)
{
    for (const CppMod *cppModPtr : cppModVec)
    {
        const CppMod &cppMod = *cppModPtr;
        if (cppMod.newlyAdded)
        {
            return;
        }

        char *ptr = const_cast<char *>(bTargetCaches[cppMod.cacheIndex].getBuildCache().data());
        if (calledFromConfiguration)
        {
            uint32_t bytesRead = 1; // (1 headerStatusChanged)
            const uint32_t headerFilesSize = readUint32(ptr, bytesRead);
            for (uint32_t i = 0; i < headerFilesSize; ++i)
            {
                Node *headerNode = readHalfNode(ptr, bytesRead);

                if (auto it = configuration->nodesType.find(headerNode); it != configuration->nodesType.end())
                {
                    if (it->second != FileType::HEADER_FILE)
                    {
                        *ptr = true;
                        return;
                    }
                }
                else
                {
                    *ptr = true;
                    return;
                }
            }
            return;
        }

        uint32_t bytesRead = 1; // (1 headerStatusChanged)
        const uint32_t headerFilesSize = readUint32(ptr, bytesRead);
        for (uint32_t i = 0; i < headerFilesSize; ++i)
        {
            Node *headerNode = readHalfNode(ptr, bytesRead);

            if (auto it = reqNodesType.find(headerNode); it != reqNodesType.end())
            {
                if (it->second != FileType::HEADER_FILE)
                {
                    *ptr = true;
                    return;
                }
            }
            else
            {
                *ptr = true;
                return;
            }
        }
    }
}

void CppTarget::setHeaderFileStatusChanged(const bool calledFromConfiguration)
{
    setHeaderFileStatusChangedCppMod(modFileDeps, calledFromConfiguration);
    setHeaderFileStatusChangedCppMod(imodFileDeps, calledFromConfiguration);
    setHeaderFileStatusChangedCppMod(huDeps, calledFromConfiguration);
}

void CppTarget::writeBigHeaderUnits()
{
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
                    if constexpr (os == OS::NT)
                    {
                        fileStr.erase(std::remove(fileStr.begin(), fileStr.end(), '\r'), fileStr.end());
                    }
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

void CppTarget::readConfigCacheAtBuildTime()
{
    const string_view configCache = bTargetCaches[cacheIndex].configCache;

    const char *ptr = configCache.data();
    uint32_t bytesRead = 0;

    RealBTarget &rb = realBTargets[0];

    const uint32_t reqVecSize = readUint32(ptr, bytesRead);
    cachedReqDeps = span{reinterpret_cast<const uint32_t *>(ptr + bytesRead), reqVecSize};
    bytesRead += 4 * reqVecSize;

    hasObjectFiles = readBool(ptr, bytesRead);

    const uint32_t sourceSize = readUint32(ptr, bytesRead);
    srcFileDeps.reserve(sourceSize);
    for (uint32_t i = 0; i < sourceSize; ++i)
    {
        CppSrc *cppSrc = new CppSrc(this, readHalfNode(ptr, bytesRead), CppModType::CPP_SRC);
        srcFileDeps.emplace_back(cppSrc);
        rb.addDep<BTargetType::CPP_SRC>(&cppSrc->realBTargets[0]);
    }

    const uint32_t modSize = readUint32(ptr, bytesRead);
    modFileDeps.reserve(modSize);
    for (uint32_t i = 0; i < modSize; ++i)
    {
        CppMod *cppMod = new CppMod(this, readHalfNode(ptr, bytesRead), CppModType::PRIMARY_IMPLEMENTATION);
        modFileDeps.emplace_back(cppMod);
        rb.addDep<BTargetType::CPP_MOD>(&cppMod->realBTargets[0]);
    }

    const uint32_t imodSize = readUint32(ptr, bytesRead);
    imodFileDeps.reserve(imodSize);
    for (uint32_t i = 0; i < imodSize; ++i)
    {
        Node *node = readHalfNode(ptr, bytesRead);
        CppModType type = readBool(ptr, bytesRead) ? CppModType::PRIMARY_EXPORT : CppModType::PARTITION_EXPORT;
        CppMod *cppMod = new CppMod(this, node, type);
        imodFileDeps.emplace_back(cppMod);
        rb.addDep<BTargetType::CPP_MOD>(&cppMod->realBTargets[0]);
    }

    const uint32_t huSize = readUint32(ptr, bytesRead);
    huDeps.reserve(huSize);
    const bool selectiveBuildLocal = configuration->evaluate(UseConfigurationScope::YES);
    for (uint32_t i = 0; i < huSize; ++i)
    {
        CppMod *hu = new CppMod(this, readHalfNode(ptr, bytesRead), CppModType::HEADER_UNIT);
        huDeps.emplace_back(hu);
        rb.addDep<BTargetType::CPP_MOD, RelationType::SELECTIVE>(&hu->realBTargets[0]);

        if (selectiveBuildLocal)
        {
            // if UseConfigurationScope is true, then we might depend on a hu from a dependent target whose
            // selectiveBuild might not true while ours is true.
            hu->selectiveBuild = selectiveBuildLocal;
        }
    }

    myBuildDir = readHalfNode(ptr, bytesRead);

    if (configuration->evaluate(IsCppMod::NO) || !useIPC)
    {
        readInclDirsAtBuildTime(ptr, bytesRead, reqIncls, isSystem);
        readInclDirsAtBuildTime(ptr, bytesRead, useReqIncls, isSystem);
    }

    if (configuration->evaluate(IsCppMod::YES))
    {
        {
            // reqHeaderNameMapping
            const uint32_t includeSize = readUint32(ptr, bytesRead);
            for (uint32_t i = 0; i < includeSize; ++i)
            {
                string_view name = readStringView(ptr, bytesRead);
                if (Node *node = readHalfNode(ptr, bytesRead);
                    !reqHeaderNameMapping.emplace(name, HeaderFileOrUnit{node, isSystem}).second)
                {
                    HMAKE_HMAKE_INTERNAL_ERROR
                }
            }
        }

        // useReqHeaderNameMapping always goes to the configuration->headerNameMapping
        const uint32_t includeSize = readUint32(ptr, bytesRead);
        for (uint32_t i = 0; i < includeSize; ++i)
        {
            string_view name = readStringView(ptr, bytesRead);
            Node *node = readHalfNode(ptr, bytesRead);
            const auto &[it, ok] = configuration->headerNameMapping.emplace(name, vector<HeaderFileOrUnit>{});
            it->second.emplace_back(cacheIndex, node, isSystem);
        }
    }

    if (bytesRead != configCache.size())
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }
}

string CppTarget::getPrintName() const
{
    return "CppTarget " + configureNode->filePath + slashc + name;
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

CppSrc &CppTarget::getCppSrc(const string &str)
{
    const string normalized = getNormalizedPath(str);
    for (CppSrc *cppSrc : srcFileDeps)
    {
        if (compareStringsFromEnd(cppSrc->node->filePath, normalized))
        {
            return *cppSrc;
        }
    }
    printErrorMessage(FORMAT("Could not find the CppSrc \n{}\nin srcFileDeps.\nin Target {}", str, name));
}

CppMod &CppTarget::getCppInterfaceModule(const string &str)
{
    const string normalized = getNormalizedPath(str);
    for (CppMod *cppMod : imodFileDeps)
    {
        if (!cppMod)
        {
            continue;
        }
        if (compareStringsFromEnd(cppMod->node->filePath, normalized))
        {
            return *cppMod;
        }
    }
    printErrorMessage(FORMAT("Could not find the CppMod \n{}\nin imodFileDeps.\nin Target {}", str, name));
    std::unreachable();
}

CppMod &CppTarget::getCppModule(const string &str)
{
    const string normalized = getNormalizedPath(str);
    for (CppMod *cppMod : modFileDeps)
    {
        if (!cppMod)
        {
            continue;
        }
        if (compareStringsFromEnd(cppMod->node->filePath, normalized))
        {
            return *cppMod;
        }
    }
    printErrorMessage(FORMAT("Could not find the CppMod \n{}\nin modFileDeps.\nin Target {}", str, name));
    std::unreachable();
}

CppMod &CppTarget::getCppHeaderUnit(const string &str, const bool addInReq, const bool addInUseReq)
{
    string includeName = str;
    lowerCaseOnWindows(includeName.data(), includeName.size());
    if (addInReq)
    {
        if (const auto it = reqHeaderNameMapping.find(includeName);
            it != reqHeaderNameMapping.end() && it->second.isUnit)
        {
            return *it->second.data.cppMod;
        }
    }

    if (addInUseReq)
    {
        if (const auto it = useReqHeaderNameMapping.find(includeName);
            it != useReqHeaderNameMapping.end() && it->second.isUnit)
        {
            return *it->second.data.cppMod;
        }
    }
    printErrorMessage(FORMAT("Could not find the Header-Unit \n{}\nin huDeps.\nin Target {}", str, name));
    std::unreachable();
}

void CppTarget::setCompileCommand(std::pmr::string &compileCommand)
{
    Compiler &compiler = configuration->compilerFeatures.compiler;

    auto getIncludeFlag = [&compiler](const bool isStandard) {
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
                str += "-isystem \"";
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
            compileCommand += "/D";
            compileCommand += i.name;
            compileCommand += '=';
            compileCommand += i.value;
            compileCommand += ' ';
        }
        else
        {
            compileCommand += "-D";
            compileCommand += i.name;
            compileCommand += '=';
            compileCommand += i.value;
            compileCommand += ' ';
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
        compileCommand += getIncludeFlag(inclNode.isStandard);
        compileCommand += inclNode.node->filePath;
        compileCommand += "\" ";
    }
}

string CppTarget::getDependenciesString() const
{
    string deps;
    if constexpr (bsMode == BSMode::BUILD)
    {
        for (const uint32_t targetIndex : cachedReqDeps)
        {
            const CppTarget *req = static_cast<CppTarget *>(bTargetCaches[targetIndex].bTarget);
            deps += req->name + '\n';
        }
        return deps;
    }

    for (CppTarget *cppTarget : reqDeps)
    {
        deps += cppTarget->name + '\n';
    }
    return deps;
}

void CppTarget::verifyConfigCache(const string_view configCache) const
{
    uint32_t bytesRead = 0;

    const uint32_t cachedReqDepsSize = readUint32(configCache.data(), bytesRead);
    if (reqDeps.size() != cachedReqDepsSize)
    {
        printErrorMessage(FORMAT("{} configCache-verification failed: reqDeps size mismatch current={} cached={}\n",
                                 getPrintName(), reqDeps.size(), cachedReqDepsSize));
    }

    uint32_t reqDepsCount = 0;
    for (const CppTarget *r : reqDeps)
    {
        if (const uint32_t cachedCacheIndex = readUint32(configCache.data(), bytesRead);
            r->cacheIndex != cachedCacheIndex)
        {
            printErrorMessage(FORMAT("{} configCache-verification failed: reqDeps[{}] cacheIndex mismatch\n"
                                     "current={}  cached={}\n",
                                     getPrintName(), reqDepsCount, r->cacheIndex, cachedCacheIndex));
        }
        ++reqDepsCount;
    }

    for (uint32_t i = 0; i < cachedReqDepsSize; ++i)
    {
    }

    const bool cachedHasObjFiles = readBool(configCache.data(), bytesRead);
    const bool hasObjFiles = !srcFileDeps.empty() || !modFileDeps.empty() || !imodFileDeps.empty();
    if (hasObjFiles != cachedHasObjFiles)
    {
        printErrorMessage(FORMAT("{} configCache-verification failed: hasObjFiles mismatch current={} cached={}\n",
                                 getPrintName(), hasObjFiles, cachedHasObjFiles));
    }

    const uint32_t cachedSrcFileDepsSize = readUint32(configCache.data(), bytesRead);
    if (srcFileDeps.size() != cachedSrcFileDepsSize)
    {
        printErrorMessage(FORMAT("{} configCache-verification failed: srcFileDeps size mismatch current={} cached={}\n",
                                 getPrintName(), srcFileDeps.size(), cachedSrcFileDepsSize));
    }

    for (uint32_t i = 0; i < cachedSrcFileDepsSize; ++i)
    {
        const Node *cachedNode = readHalfNode(configCache.data(), bytesRead);
        if (i < srcFileDeps.size() && srcFileDeps[i]->node != cachedNode)
        {
            printErrorMessage(FORMAT("{} configCache-verification failed: srcFileDeps[{}] node mismatch\n"
                                     "current=\"{}\"  cached=\"{}\"\n",
                                     getPrintName(), i,
                                     srcFileDeps[i]->node ? srcFileDeps[i]->node->filePath : "<null>",
                                     cachedNode ? cachedNode->filePath : "<null>"));
        }
    }

    const uint32_t cachedModFileDepsSize = readUint32(configCache.data(), bytesRead);
    if (modFileDeps.size() != cachedModFileDepsSize)
    {
        printErrorMessage(FORMAT("{} configCache-verification failed: modFileDeps size mismatch current={} cached={}\n",
                                 getPrintName(), modFileDeps.size(), cachedModFileDepsSize));
    }

    for (uint32_t i = 0; i < cachedModFileDepsSize; ++i)
    {
        const Node *cachedNode = readHalfNode(configCache.data(), bytesRead);
        if (i < modFileDeps.size() && modFileDeps[i]->node != cachedNode)
        {
            printErrorMessage(FORMAT("{} configCache-verification failed: modFileDeps[{}] node mismatch\n"
                                     "current=\"{}\"  cached=\"{}\"\n",
                                     getPrintName(), i,
                                     modFileDeps[i]->node ? modFileDeps[i]->node->filePath : "<null>",
                                     cachedNode ? cachedNode->filePath : "<null>"));
        }
    }

    const uint32_t cachedImodFileDepsSize = readUint32(configCache.data(), bytesRead);
    if (imodFileDeps.size() != cachedImodFileDepsSize)
    {
        printErrorMessage(
            FORMAT("{} configCache-verification failed: imodFileDeps size mismatch current={} cached={}\n",
                   getPrintName(), imodFileDeps.size(), cachedImodFileDepsSize));
    }

    for (uint32_t i = 0; i < cachedImodFileDepsSize; ++i)
    {
        const Node *cachedNode = readHalfNode(configCache.data(), bytesRead);
        if (i < imodFileDeps.size() && imodFileDeps[i]->node != cachedNode)
        {
            printErrorMessage(FORMAT("{} configCache-verification failed: imodFileDeps[{}] node mismatch\n"
                                     "current=\"{}\"  cached=\"{}\"\n",
                                     getPrintName(), i,
                                     imodFileDeps[i]->node ? imodFileDeps[i]->node->filePath : "<null>",
                                     cachedNode ? cachedNode->filePath : "<null>"));
        }

        const bool cachedIsPrimaryExport = readBool(configCache.data(), bytesRead);
        if (i < imodFileDeps.size() && (imodFileDeps[i]->type == CppModType::PRIMARY_EXPORT) != cachedIsPrimaryExport)
        {
            printErrorMessage(FORMAT("{} configCache-verification failed: imodFileDeps[{}] isPrimaryExport mismatch\n"
                                     "current={}  cached={}\n",
                                     getPrintName(), i, imodFileDeps[i]->type == CppModType::PRIMARY_EXPORT,
                                     cachedIsPrimaryExport));
        }
    }

    const uint32_t cachedHuDepsSize = readUint32(configCache.data(), bytesRead);
    if (huDeps.size() != cachedHuDepsSize)
    {
        printErrorMessage(FORMAT("{} configCache-verification failed: huDeps size mismatch current={} cached={}\n",
                                 getPrintName(), huDeps.size(), cachedHuDepsSize));
    }

    for (uint32_t i = 0; i < cachedHuDepsSize; ++i)
    {
        const Node *cachedNode = readHalfNode(configCache.data(), bytesRead);
        if (i < huDeps.size() && huDeps[i]->node != cachedNode)
        {
            printErrorMessage(FORMAT("{} configCache-verification failed: huDeps[{}] node mismatch\n"
                                     "current=\"{}\"  cached=\"{}\"\n",
                                     getPrintName(), i, huDeps[i]->node ? huDeps[i]->node->filePath : "<null>",
                                     cachedNode ? cachedNode->filePath : "<null>"));
        }
    }

    const Node *cachedMyBuildDir = readHalfNode(configCache.data(), bytesRead);
    if (myBuildDir != cachedMyBuildDir)
    {
        printErrorMessage(FORMAT("{} configCache-verification failed: myBuildDir mismatch\n"
                                 "current=\"{}\"  cached=\"{}\"\n",
                                 getPrintName(), myBuildDir ? myBuildDir->filePath : "<null>",
                                 cachedMyBuildDir ? cachedMyBuildDir->filePath : "<null>"));
    }

    if (configuration->evaluate(IsCppMod::NO) || !useIPC)
    {
        const uint32_t cachedReqInclsSize = readUint32(configCache.data(), bytesRead);
        if (reqIncls.size() != cachedReqInclsSize)
        {
            printErrorMessage(
                FORMAT("{} configCache-verification failed: reqIncls size mismatch current={} cached={}\n",
                       getPrintName(), reqIncls.size(), cachedReqInclsSize));
        }

        for (uint32_t i = 0; i < cachedReqInclsSize; ++i)
        {
            const Node *cachedNode = readHalfNode(configCache.data(), bytesRead);
            if (i < reqIncls.size() && reqIncls[i].node != cachedNode)
            {
                printErrorMessage(FORMAT("{} configCache-verification failed: reqIncls[{}] node mismatch\n"
                                         "current=\"{}\"  cached=\"{}\"\n",
                                         getPrintName(), i, reqIncls[i].node ? reqIncls[i].node->filePath : "<null>",
                                         cachedNode ? cachedNode->filePath : "<null>"));
            }
        }

        const uint32_t cachedUseReqInclsSize = readUint32(configCache.data(), bytesRead);
        if (useReqIncls.size() != cachedUseReqInclsSize)
        {
            printErrorMessage(
                FORMAT("{} configCache-verification failed: useReqIncls size mismatch current={} cached={}\n",
                       getPrintName(), useReqIncls.size(), cachedUseReqInclsSize));
        }

        for (uint32_t i = 0; i < cachedUseReqInclsSize; ++i)
        {
            const Node *cachedNode = readHalfNode(configCache.data(), bytesRead);
            if (i < useReqIncls.size() && useReqIncls[i].node != cachedNode)
            {
                printErrorMessage(FORMAT("{} configCache-verification failed: useReqIncls[{}] node mismatch\n"
                                         "current=\"{}\"  cached=\"{}\"\n",
                                         getPrintName(), i,
                                         useReqIncls[i].node ? useReqIncls[i].node->filePath : "<null>",
                                         cachedNode ? cachedNode->filePath : "<null>"));
            }
        }
    }

    if (configuration->evaluate(IsCppMod::YES))
    {
        auto verifyHeaderNameMapping = [&](const flat_hash_map<string_view, HeaderFileOrUnit> &mapping,
                                           const string_view mapName) {
            const uint32_t cachedCount = readUint32(configCache.data(), bytesRead);
            uint32_t verifiedCount = 0;

            for (uint32_t i = 0; i < cachedCount; ++i)
            {
                const string_view cachedName = readStringView(configCache.data(), bytesRead);
                const Node *cachedNode = readHalfNode(configCache.data(), bytesRead);

                const auto it = mapping.find(cachedName);
                if (it == mapping.end())
                {
                    printErrorMessage(FORMAT("{} configCache-verification failed: {}[{}] cached name\n\"{}\"\n"
                                             "not found in current mapping\n",
                                             getPrintName(), mapName, i, cachedName));
                }
                else if (it->second.data.node != cachedNode)
                {
                    printErrorMessage(FORMAT("{} configCache-verification failed: {}[\"{}\"] node mismatch\n"
                                             "current=\"{}\"  cached=\"{}\"\n",
                                             getPrintName(), mapName, cachedName,
                                             it->second.data.node ? it->second.data.node->filePath : "<null>",
                                             cachedNode ? cachedNode->filePath : "<null>"));
                }
                ++verifiedCount;
            }

            // verify cached count matches actual non-unit count in current mapping
            uint32_t currentNonUnitCount = 0;
            for (const auto &[s, h] : mapping)
            {
                if (!h.isUnit)
                {
                    ++currentNonUnitCount;
                }
            }
            if (currentNonUnitCount != cachedCount)
            {
                printErrorMessage(FORMAT("{} configCache-verification failed: {} non-unit count mismatch\n"
                                         "current={}  cached={}\n",
                                         getPrintName(), mapName, currentNonUnitCount, cachedCount));
            }
        };

        verifyHeaderNameMapping(reqHeaderNameMapping, "reqHeaderNameMapping");
        verifyHeaderNameMapping(useReqHeaderNameMapping, "useReqHeaderNameMapping");
    }

    if (configCache.size() != bytesRead)
    {
        printErrorMessage(FORMAT("{} configCache-verification failed as configCache.size() != bytesRead {} vs {}\n",
                                 getPrintName(), configCache.size(), bytesRead));
    }
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
