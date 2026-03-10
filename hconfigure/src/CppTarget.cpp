
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
    : targetIndex{targetIndex_}, data{.cppMod = cppMod_}, isUnit(true), isSystem(isSystem_)
{
}
HeaderFileOrUnit::HeaderFileOrUnit(const uint32_t targetIndex_, Node *node_, bool isSystem_)
    : targetIndex{targetIndex_}, data{.node = node_}, isUnit(false), isSystem(isSystem_)
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

void writeIncDirsAtConfigTime(string &buffer, const vector<InclNode> &include)
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

void writeHeaderFilesAtConfigTime(string &buffer, flat_hash_map<string_view, HeaderFileOrUnit> &headerNameMapping,
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
        readCacheAtBuildTime();
        populateTransitiveProperties();
        initSourceCache();
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
        tempNameMapping.merge(reqHeaderNameMapping);
        tempNameMapping.merge(useReqHeaderNameMapping);

        for (const auto &[headerName, type] : tempNameMapping)
        {
            const auto &[it, ok] = configuration->headerNameMapping.emplace(headerName, vector(1, type));
            const HeaderFileOrUnit headerFileOrUnit = it->second[0];
            if (!ok)
            {
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
        if (!type.isUnit)
        {
            ++(addInReq ? reqHeaderFilesSize : useReqHeaderFilesSize);
        }
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

    addHeaderUnit(*p, headerNode, addInReq, addInUseReq);
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
        reqNodesType.erase(headerNode);
    }

    if (addInUseReq)
    {
        useReqNodesType.erase(headerNode);
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

        hu = huDeps.emplace_back(new CppMod(this, headerUnit));

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
        publicBigHus[index] = new CppMod(this, bigHuNode);
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
        privateBigHus[index] = new CppMod(this, bigHuNode);
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
        interfaceBigHus[index] = new CppMod(this, bigHuNode);
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
    headerNames += "crtdbg.h,ntverp.h,";

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

void CppTarget::initSourceCache()
{
    if (name == "DLDebugInfoDIContext-cpp")
    {
        bool brekapoint = true;
    }
    constexpr uint32_t stackSize = 64 * 1024;

    char cppBuffer[stackSize];
    char cBuffer[stackSize];
    char assemblyBuffer[stackSize];

    std::pmr::monotonic_buffer_resource cppAlloc(cppBuffer, stackSize);
    std::pmr::monotonic_buffer_resource cAlloc(cppBuffer, stackSize);
    std::pmr::monotonic_buffer_resource assemblyAlloc(cppBuffer, stackSize);

    std::pmr::string cppFullCompileCommand(&cppAlloc);
    std::pmr::string cFullCompileCommand(&cAlloc);
    std::pmr::string assemblyFullCompileCommand(&assemblyAlloc);

    HashedCommand cppHashCommand;
    HashedCommand cHashCommand;
    HashedCommand assemblyHashCommand;

    auto setCompileCommandSourceType = [&](const SourceType sourceType) {
        if (sourceType == SourceType::CPP && cppFullCompileCommand.empty())
        {
            cppFullCompileCommand = configuration->cppCompileCommand;
            setCompileCommand(cppFullCompileCommand);
            cppHashCommand.setCommand(cppFullCompileCommand);
            return cppHashCommand.getHash();
        }
        if (sourceType == SourceType::C)
        {
            cFullCompileCommand = configuration->cCompileCommand;
            setCompileCommand(cFullCompileCommand);
            cHashCommand.setCommand(cFullCompileCommand);
            return cppHashCommand.getHash();
        }
        assemblyFullCompileCommand = configuration->assemblyCompileCommand;
        setCompileCommand(assemblyFullCompileCommand);
        assemblyHashCommand.setCommand(assemblyFullCompileCommand);
        return cppHashCommand.getHash();
    };

    for (uint32_t i = 0; i < srcFileDeps.size(); ++i)
    {
        const uint64_t commandHash = setCompileCommandSourceType(srcFileDeps[i]->setSourceType());
        srcFileDeps[i]->initializeBuildCache(i, commandHash);
    }
    for (uint32_t i = 0; i < modFileDeps.size(); ++i)
    {
        const uint64_t commandHash = setCompileCommandSourceType(modFileDeps[i]->setSourceType());
        modFileDeps[i]->initializeBuildCache(cppBuildCache.modFiles[i], i, commandHash);
    }
    for (uint32_t i = 0; i < imodFileDeps.size(); ++i)
    {
        if (imodFileDeps[i])
        {
            const uint64_t commandHash = setCompileCommandSourceType(SourceType::CPP);
            imodFileDeps[i]->initializeBuildCache(cppBuildCache.imodFiles[i], i, commandHash);
        }
    }
    for (uint32_t i = 0; i < huDeps.size(); ++i)
    {
        if (huDeps[i])
        {
            const uint64_t commandHash = setCompileCommandSourceType(SourceType::CPP);
            huDeps[i]->initializeBuildCache(cppBuildCache.headerUnits[i], i, commandHash);
        }
    }
}

void CppTarget::completeRoundOne()
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        writeBigHeaderUnits();
        populateReqAndUseReqDeps();
        writeCacheAtConfigTime();
        populateNameMappingsAndNodesType();
        if (configuration->evaluate(UseConfigurationScope::NO))
        {
            setHeaderFileStatusChanged(false);
        }
        populateTransitiveProperties();
        return;
    }
}

bool CppTarget::writeBuildCache(string &buffer)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        cppBuildCache.serialize(buffer);
        return true;
    }

    if (buildCacheUpdated)
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

void CppTarget::setHeaderFileStatusChangedCppMod(BuildCache::Cpp::ModuleFile &modCache, bool calledFromConfiguration)
{
    if (calledFromConfiguration)
    {
        for (Node *node : modCache.srcFile.headerFiles)
        {
            if (auto it = configuration->nodesType.find(node); it != configuration->nodesType.end())
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
        return;
    }

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
}

void CppTarget::setHeaderFileStatusChanged(const bool calledFromConfiguration)
{
    for (uint32_t i = 0; i < modFileDeps.size(); ++i)
    {
        setHeaderFileStatusChangedCppMod(cppBuildCache.modFiles[i], calledFromConfiguration);
    }

    for (uint32_t i = 0; i < imodFileDeps.size(); ++i)
    {
        setHeaderFileStatusChangedCppMod(cppBuildCache.imodFiles[i], calledFromConfiguration);
    }

    for (CppMod *hu : huDeps)
    {
        setHeaderFileStatusChangedCppMod(cppBuildCache.headerUnits[hu->myBuildCacheIndex], calledFromConfiguration);
    }
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
    auto *configBuffer = new string{};

    const bool hasObjFiles = !srcFileDeps.empty() || !modFileDeps.empty() || !imodFileDeps.empty();
    writeBool(*configBuffer, hasObjFiles);

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

        if (useIPC)
        {
            for (const auto &[headerName, headerNode] : hu->composingHeaders)
            {
                writeStringView(*configBuffer, headerName);
                writeNode(*configBuffer, headerNode);
            }
        }
        else
        {
            for (const auto &[headerName, headerNode] : hu->composingHeaders)
            {
                writeStringView(*configBuffer, headerName);
            }
        }

        for (const string &str : hu->logicalNames)
        {
            writeStringView(*configBuffer, str);
        }
    }

    writeNode(*configBuffer, myBuildDir);

    if (configuration->evaluate(IsCppMod::NO) || !useIPC)
    {
        writeIncDirsAtConfigTime(*configBuffer, reqIncls);
        writeIncDirsAtConfigTime(*configBuffer, useReqIncls);
    }

    if (configuration->evaluate(IsCppMod::YES))
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
    uint32_t bytesRead = 0;

    hasObjectFiles = readBool(ptr, bytesRead);

    const uint32_t reqVecSize = readUint32(ptr, bytesRead);
    for (uint32_t i = 0; i < reqVecSize; ++i)
    {
        CppTarget *req = static_cast<CppTarget *>(fileTargetCaches[readUint32(ptr, bytesRead)].targetCache);
        reqDeps.emplace(req);
    }
    const uint32_t sourceSize = readUint32(ptr, bytesRead);
    srcFileDeps.reserve(sourceSize);
    for (uint32_t i = 0; i < sourceSize; ++i)
    {
        CppSrc *src = srcFileDeps.emplace_back(new CppSrc(this, readHalfNode(ptr, bytesRead)));
        src->objectNode = readHalfNode(ptr, bytesRead);

        addDep<0>(*srcFileDeps[i]);
    }

    const uint32_t modSize = readUint32(ptr, bytesRead);
    modFileDeps.reserve(modSize);
    for (uint32_t i = 0; i < modSize; ++i)
    {
        CppMod *cppMod = modFileDeps.emplace_back(new CppMod(this, readHalfNode(ptr, bytesRead)));
        cppMod->objectNode = readHalfNode(ptr, bytesRead);
        cppMod->type = SM_FILE_TYPE::PRIMARY_IMPLEMENTATION;

        addDep<0>(*cppMod);
    }

    imodFileDeps.resize(cppBuildCache.imodFiles.size());

    const uint32_t imodSize = readUint32(ptr, bytesRead);
    for (uint32_t i = 0; i < imodSize; ++i)
    {
        const uint32_t indexInBuildCache = readUint32(ptr, bytesRead);
        CppMod *cppMod = imodFileDeps[indexInBuildCache] = new CppMod(this, readHalfNode(ptr, bytesRead));
        cppMod->myBuildCacheIndex = indexInBuildCache;

        cppMod->logicalNames.emplace_back(readStringView(ptr, bytesRead));
        cppMod->objectNode = readHalfNode(ptr, bytesRead);
        cppMod->interfaceNode = readHalfNode(ptr, bytesRead);
        cppMod->type =
            cppMod->logicalNames[0].contains(':') ? SM_FILE_TYPE::PARTITION_EXPORT : SM_FILE_TYPE::PRIMARY_EXPORT;
        imodNames.emplace(cppMod->logicalNames[0], cppMod);

        addDep<0>(*cppMod);
    }

    huDeps.resize(cppBuildCache.headerUnits.size());

    bool selectiveBuildLocal = configuration->evaluate(UseConfigurationScope::YES);
    const uint32_t huSize = readUint32(ptr, bytesRead);
    for (uint32_t i = 0; i < huSize; ++i)
    {
        const uint32_t indexInBuildCache = readUint32(ptr, bytesRead);
        CppMod *hu = huDeps[indexInBuildCache] = new CppMod(this, readHalfNode(ptr, bytesRead));
        if (selectiveBuildLocal)
        {
            // if UseConfigurationScope is true, then we might depend on a hu from a dependent target whose
            // selectiveBuild might not true while ours is true.
            hu->selectiveBuild = selectiveBuildLocal;
        }
        hu->myBuildCacheIndex = indexInBuildCache;
        hu->interfaceNode = readHalfNode(ptr, bytesRead);

        hu->isReqHu = readBool(ptr, bytesRead);
        hu->isUseReqHu = readBool(ptr, bytesRead);

        const uint32_t headerFileModuleSize = readUint32(ptr, bytesRead);
        const uint32_t logicalNamesSize = readUint32(ptr, bytesRead);
        hu->logicalNames.reserve(logicalNamesSize + headerFileModuleSize);

        for (uint32_t j = 0; j < headerFileModuleSize; ++j)
        {
            string_view headerFileName = readStringView(ptr, bytesRead);
            if (useIPC)
            {
                Node *headerNode = readHalfNode(ptr, bytesRead);
                hu->composingHeaders.emplace(headerFileName, headerNode);
            }
            else
            {
                hu->composingHeaders.emplace(headerFileName, nullptr);
            }
            hu->logicalNames.emplace_back(headerFileName);

            if (hu->isReqHu)
            {
                reqHeaderNameMapping.emplace(headerFileName, HeaderFileOrUnit(hu, isSystem));
            }

            if (hu->isUseReqHu)
            {
                const auto &[it, ok] =
                    configuration->headerNameMapping.emplace(headerFileName, vector<HeaderFileOrUnit>{});
                it->second.emplace_back(cacheIndex, hu, isSystem);
            }
        }

        for (uint32_t j = 0; j < logicalNamesSize; ++j)
        {
            string_view str = readStringView(ptr, bytesRead);
            hu->logicalNames.emplace_back(str);
            if (hu->isReqHu)
            {
                reqHeaderNameMapping.emplace(str, HeaderFileOrUnit(hu, isSystem));
            }

            if (hu->isUseReqHu)
            {
                const auto &[it, ok] = configuration->headerNameMapping.emplace(str, vector<HeaderFileOrUnit>{});
                it->second.emplace_back(cacheIndex, hu, isSystem);
            }
        }

        hu->type = SM_FILE_TYPE::HEADER_UNIT;
        addDep<0, BTargetDepType::SELECTIVE>(*hu);
    }

    myBuildDir = readHalfNode(ptr, bytesRead);

    if (configuration->evaluate(IsCppMod::NO) || !useIPC)
    {
        readInclDirsAtBuildTime(ptr, bytesRead, reqIncls, isSystem, ignoreHeaderDeps);
        readInclDirsAtBuildTime(ptr, bytesRead, useReqIncls, isSystem, ignoreHeaderDeps);
    }

    if (configuration->evaluate(IsCppMod::YES))
    {
        readHeaderFilesAtBuildTime(ptr, bytesRead, reqHeaderNameMapping, isSystem);
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

bool CppTarget::isEventRegistered(Builder &builder)
{
    // This is necessary since objectFile->outputFileNode is not updated once after it is compiled.
    if (realBTargets[0].updateStatus == UpdateStatus::NEEDS_UPDATE)
    {
        realBTargets[0].assignNeedsUpdateToDependents();
    }
    return false;
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
    for (CppTarget *cppTarget : reqDeps)
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
