
#include "CppTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "ConfigurationAssign.hpp"
#include "LOAT.hpp"
#include "rapidhash/rapidhash.h"
#include <filesystem>
#include <fstream>
#include <memory_resource>
#include <regex>
#include <utility>

using std::filesystem::create_directories, std::filesystem::directory_iterator,
    std::filesystem::recursive_directory_iterator, std::ifstream, std::ofstream, std::regex, std::regex_error;

namespace
{
SourceType sourceTypeOf(const string_view path)
{
    if (path.ends_with(".c"))
    {
        return SourceType::C;
    }
    if (path.ends_with(".S") || path.ends_with(".s"))
    {
        return SourceType::ASSEMBLY;
    }
    return SourceType::CPP;
}

CppSrc *findExistingCompileUnit(const CppTarget &target, const Node &node, const CppModType type)
{
    const uint64_t cacheName = static_cast<uint64_t>(node.myId) << 32 | static_cast<uint64_t>(target.cacheIndex) << 3 |
                               static_cast<uint64_t>(type);
    const auto cacheIt = nameToIndexMap.find(cacheName);
    if (cacheIt == nameToIndexMap.end())
    {
        return nullptr;
    }
    return static_cast<CppSrc *>(bTargetCaches[cacheIt->second].bTarget);
}
} // namespace

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
        string_view line = string_view(str).substr(start, i - start);

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
                        printErrorMessage(
                            FORMAT("Module-map section is declared more than once.\nFile: {}\nSection: {}",
                                   dir + "module-map.txt", modeStrs[newModeIndex]));
                    }
                    printErrorMessage(FORMAT("Module-map sections are out of order.\nFile: {}\nSection: {}\n"
                                             "Previous section index: {}\nCurrent section index: {}",
                                             dir + "module-map.txt", modeStrs[newModeIndex], currentModeIndex,
                                             newModeIndex));
                }

                if (!pendingLogicalName.empty())
                {
                    printErrorMessage(FORMAT("Module-map entry is missing its file path.\nFile: {}\nLogical name: {}",
                                             dir + "module-map.txt", pendingLogicalName));
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
            printErrorMessage(FORMAT("Module-map data appears before the first section declaration.\nFile: {}\n"
                                     "Value: {}",
                                     dir + "module-map.txt", line));
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
            printErrorMessage(
                FORMAT("Module-map entry refers to a missing file.\nModule map: {}\nSection: {}\nPath: {}",
                       dir + "module-map.txt", modeStrs[currentModeIndex], node->filePath));
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

CppTarget::CppTarget(const string &name_, Configuration *configuration_)
    : ObjectFileProducer(name_, BTargetType::CPP_TARGET, false, false), configuration(configuration_)
{
    initializeCppTarget(name_, nullptr);
}

CppTarget::CppTarget(const bool buildExplicit, const string &name_, Configuration *configuration_)
    : ObjectFileProducer(name_, BTargetType::CPP_TARGET, buildExplicit, false), configuration(configuration_)
{
    initializeCppTarget(name_, nullptr);
}

CppTarget::CppTarget(Node *myBuildDir_, const string &name_, Configuration *configuration_)
    : ObjectFileProducer(name_, BTargetType::CPP_TARGET, false, false), configuration(configuration_)
{
    initializeCppTarget(name_, myBuildDir_);
}

CppTarget::CppTarget(Node *myBuildDir_, const bool buildExplicit, const string &name_, Configuration *configuration_)
    : ObjectFileProducer(name_, BTargetType::CPP_TARGET, buildExplicit, false), configuration(configuration_)
{
    initializeCppTarget(name_, myBuildDir_);
}

void writeIncDirsAtConfigTime(string &buffer, const vector<InclNode> &include)
{
    writeUint32(buffer, include.size());
    for (const InclNode &inclNode : include)
    {
        writeNode(buffer, inclNode.node);
        writeBool(buffer, inclNode.isStandard);
    }
}

void readInclDirsAtBuildTime(const char *ptr, uint64_t &bytesRead, vector<InclNode> &include)
{
    const uint32_t reserveSize = readUint32(ptr, bytesRead);
    include.reserve(reserveSize);
    for (uint32_t i = 0; i < reserveSize; ++i)
    {
        Node *node = readHalfNode(ptr, bytesRead);
        include.emplace_back(node, readBool(ptr, bytesRead));
    }
}

void writeHeaderFilesAtConfigTime(string &buffer, const flat_hash_map<string_view, HfOrCppMod> &headerNameMapping)
{
    // Reserve space for the count, fill it in after iteration.
    const uint64_t countOffset = buffer.size();
    writeUint32(buffer, 0);

    uint32_t written = 0;
    for (const auto &[s, h] : headerNameMapping)
    {
        if (h.type != FileType::HEADER_FILE)
        {
            continue;
        }
        writeStringView(buffer, s);
        writeNode(buffer, h.data.node);
        assert(written != static_cast<uint32_t>(-1));
        ++written;
    }

    memcpy(buffer.data() + countOffset, &written, sizeof(written));
}

void CppTarget::initializeCppTarget(const string &name_, Node *myBuildDir_)
{
    isCppTarget = true;
    isSystem = configuration->evaluate(SystemTarget::YES);
    useIPC = configuration->evaluate(UseIPC::YES);
    jumboBuild = configuration->jumboBuild;
    jumboFileSize = configuration->jumboFileSize;
    addCppSource = configuration->addCppSource;

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

AdaptiveManager &CppTarget::getOrCreateAdaptiveManager()
{
    if (adaptiveManager == nullptr)
    {
        adaptiveManager = new AdaptiveManager(this);
        // Round one partitions the candidates before CppTarget computes command hashes and exposes its active object
        // files. Selected round-zero compile units will depend on the manager directly.
        realBTargets[1].addDep<BTargetType::UNKNOWN>(&adaptiveManager->realBTargets[1]);
    }
    return *adaptiveManager;
}

BTarget &CppTarget::getOrCreateBeforeTarget()
{
    if (beforeTarget != nullptr)
    {
        return *beforeTarget;
    }
    const uint64_t beforeCacheName =
        rapidhash_withSeed(&cacheName, sizeof(cacheName), 0x4245464F52450000ULL); // "BEFORE"
    beforeTarget = new BTarget(name + "/before", beforeCacheName, false, BTargetType::BEFORE_TARGET, false, false,
                               true, false);
    return *beforeTarget;
}

void CppTarget::connectBeforeTarget()
{
    if (beforeTarget == nullptr)
    {
        return;
    }

    const auto connectCompileUnit = [this](CppSrc *compileUnit) {
        // Generated jumbo units reach the barrier through AdaptiveManager. Standalone adaptive units and ordinary
        // compile units receive the direct edge.
        if (compileUnit->isAJumboBuild)
        {
            return;
        }
        compileUnit->realBTargets[0].addDep<BTargetType::BEFORE_TARGET>(&beforeTarget->realBTargets[0]);
    };

    for (CppSrc *source : srcFileDeps)
    {
        connectCompileUnit(source);
    }
    for (CppMod *module : modFileDeps)
    {
        connectCompileUnit(module);
    }
    for (CppMod *module : imodFileDeps)
    {
        connectCompileUnit(module);
    }
    for (CppMod *headerUnit : huDeps)
    {
        connectCompileUnit(headerUnit);
    }
    if (adaptiveManager != nullptr)
    {
        adaptiveManager->realBTargets[0].addDep<BTargetType::BEFORE_TARGET>(&beforeTarget->realBTargets[0]);
    }
}

void CppTarget::startJumboGroup()
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const uint32_t start = adaptiveSourceNodes.size();
        if (start != 0 && (adaptiveGroupStarts.empty() || adaptiveGroupStarts.back() != start))
        {
            adaptiveGroupStarts.emplace_back(start);
        }
    }
}

string_view CppTarget::getAdaptiveIncludeName(const Node *node) const
{
    if (srcNode == nullptr)
    {
        printErrorMessage("Adaptive unity requires a project source root (`srcNode`).");
    }

    // Node paths are already lexically normalized (and lower-cased on Windows), so deriving the include name only
    // requires removing the source-root prefix. Keep the separator check: a plain starts_with() would incorrectly
    // accept a sibling such as `/repo/project-other` for the source root `/repo/project`.
    const string_view sourceRoot = srcNode->filePath;
    const string_view sourcePath = node->filePath;
    uint64_t relativeStart = sourceRoot.size();
    const bool rootEndsInSeparator = !sourceRoot.empty() && sourceRoot.back() == slashc;
    if (!sourcePath.starts_with(sourceRoot) || sourcePath.size() <= relativeStart ||
        (!rootEndsInSeparator && sourcePath[relativeStart] != slashc))
    {
        printErrorMessage(FORMAT("Adaptive-unity source is outside the project source root.\nTarget: {}\n"
                                 "Source root: {}\nSource: {}",
                                 name, srcNode->filePath, node->filePath));
    }
    relativeStart += !rootEndsInSeparator;
    const string_view includeName = sourcePath.substr(relativeStart);
    if (includeName.contains('"'))
    {
        printErrorMessage(FORMAT("Adaptive-unity include path contains a quote.\nSource: {}", node->filePath));
    }
    return includeName;
}

void CppTarget::getObjectFiles(std::pmr::vector<Node *> &objectNodes_, const bool includeRequiredProducers) const
{
    ObjectFileProducer::getObjectFiles(objectNodes_, includeRequiredProducers);

    for (const CppMod *objectFile : modFileDeps)
    {
        objectNodes_.insert(objectNodes_.end(), objectFile->objectNodes.begin(), objectFile->objectNodes.end());
    }

    for (const CppMod *objectFile : imodFileDeps)
    {
        objectNodes_.insert(objectNodes_.end(), objectFile->objectNodes.begin(), objectFile->objectNodes.end());
    }

    for (const CppSrc *objectFile : srcFileDeps)
    {
        objectNodes_.insert(objectNodes_.end(), objectFile->objectNodes.begin(), objectFile->objectNodes.end());
    }
}

void CppTarget::populateTransitiveProperties()
{
    FOR_REQ_OBJECT_FILE_PRODUCERS(this, producer, dependency)
    {
        if (!dependency.isOpDependency() || !producer->isCppTarget)
        {
            continue;
        }
        auto *cppTarget = static_cast<CppTarget *>(producer);
        if (configuration->evaluate(IsCppMod::NO) || !useIPC)
        {
            for (const InclNode &inclNode : cppTarget->useReqIncls)
            {
                const auto existing =
                    std::ranges::find(reqIncls, inclNode.node, [](const InclNode &entry) { return entry.node; });
                if (existing == reqIncls.end())
                {
                    reqIncls.emplace_back(inclNode);
                }
                else
                {
                    // If either declaration treats the path as project code, retain -I so warnings are not hidden.
                    existing->isStandard &= inclNode.isStandard;
                }
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
    if (addCppSource == AddCppSource::NO)
    {
        return;
    }

    if (configuration->evaluate(IsCppMod::YES))
    {
        printErrorMessage(FORMAT("A regular source was added to a module-enabled target.\nTarget: {}\nSource file: {}\n"
                                 "Hint: use a moduleFiles* API for module implementation units.",
                                 name, node->filePath));
    }

    for (const CppSrc *source : srcFileDeps)
    {
        if (source->node == node)
        {
            printErrorMessage(
                FORMAT("Source file was added more than once.\nTarget: {}\nSource file: {}", name, node->filePath));
            return;
        }
    }
    if (std::ranges::find(adaptiveSourceNodes, node) != adaptiveSourceNodes.end())
    {
        printErrorMessage(
            FORMAT("Source file was added more than once.\nTarget: {}\nSource file: {}", name, node->filePath));
    }

    if (jumboBuild == JumboBuild::YES && sourceTypeOf(node->filePath) == SourceType::CPP)
    {
        adaptiveSourceNodes.emplace_back(const_cast<Node *>(node));
    }
    else
    {
        CppSrc *source = findExistingCompileUnit(*this, *node, CppModType::CPP_SRC);
        srcFileDeps.emplace_back(source != nullptr ? source : new CppSrc(this, node, CppModType::CPP_SRC));
    }
}

string CppTarget::getExportNameFromFirstLine(const Node *node)
{
    constexpr string_view kModuleTag = "// module:";

    ifstream file(node->filePath);
    if (!file)
    {
        printErrorMessage(
            FORMAT("Could not open a module file to read its module declaration.\nModule file: {}", node->filePath));
        return {};
    }

    string firstLine;
    if (!std::getline(file, firstLine))
    {
        printErrorMessage(FORMAT("Could not read the module declaration line.\nModule file: {}", node->filePath));
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
    if (addCppSource == AddCppSource::NO)
    {
        return;
    }

    if (configuration->evaluate(IsCppMod::NO))
    {
        printErrorMessage(FORMAT("A module file was added to a target with modules disabled.\nTarget: {}\n"
                                 "Module file: {}\nRequired setting: IsCppMod::YES",
                                 name, node->filePath));
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
                printErrorMessage(FORMAT("Module implementation file was added more than once.\nTarget: {}\n"
                                         "Module file: {}",
                                         name, node->filePath));
                return;
            }
        }
        if (std::ranges::find(adaptiveSourceNodes, node) != adaptiveSourceNodes.end())
        {
            printErrorMessage(FORMAT("Module implementation file was added more than once.\nTarget: {}\n"
                                     "Module file: {}",
                                     name, node->filePath));
        }
        if (jumboBuild == JumboBuild::YES && sourceTypeOf(node->filePath) == SourceType::CPP)
        {
            adaptiveSourceNodes.emplace_back(const_cast<Node *>(node));
            if (useIPC)
            {
                addHeaderFile(getAdaptiveIncludeName(node), node, true, false);
            }
        }
        else
        {
            CppSrc *source = findExistingCompileUnit(*this, *node, CppModType::PRIMARY_IMPLEMENTATION);
            modFileDeps.emplace_back(source != nullptr ? static_cast<CppMod *>(source)
                                                       : new CppMod(this, node, CppModType::PRIMARY_IMPLEMENTATION));
        }
    }
    else
    {
        for (const CppMod *cppMod : imodFileDeps)
        {
            if (cppMod->node == node)
            {
                printErrorMessage(FORMAT("Module interface file was added more than once.\nTarget: {}\n"
                                         "Module file: {}\nExport name: {}",
                                         name, node->filePath, exportName));
                return;
            }
        }
        imodFileDeps.emplace_back(new CppMod(
            this, node, exportName.contains(':') ? CppModType::PARTITION_EXPORT : CppModType::PRIMARY_EXPORT));
        imodFileDeps[imodFileDeps.size() - 1]->logicalName = *new string(exportName);
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
            if (it->second.type == FileType::HEADER_UNIT || it->second.type == FileType::MODULE)
            {
                str = FORMAT("Header logical name maps to different CppMod in different scopes.\nTarget: {}\n"
                             "Logical name: {}\nPrivate/required path: {}\nInterface path: {}",
                             name, it->first, it->second.data.cppMod->node->filePath,
                             it2->second.data.cppMod->node->filePath);
            }
            else
            {
                str = FORMAT("Header logical name maps to different files in different scopes.\nTarget: {}\n"
                             "Logical name: {}\nPrivate/required path: {}\nInterface path: {}",
                             name, it->first, it->second.data.node->filePath, it2->second.data.node->filePath);
            }
            printErrorMessage(str);
        }
    }
}

void CppTarget::populateNameMappingsAndNodesType()
{
    if (configuration->evaluate(UseConfigurationScope::YES))
    {
        flat_hash_map<string_view, HfOrCppMod> tempNameMapping;
        tempNameMapping.reserve(reqHeaderNameMapping.size() + useReqHeaderNameMapping.size());
        tempNameMapping.insert(reqHeaderNameMapping.begin(), reqHeaderNameMapping.end());
        tempNameMapping.insert(useReqHeaderNameMapping.begin(), useReqHeaderNameMapping.end());

        for (const auto &[headerName, hfOrCppMod] : tempNameMapping)
        {
            if (const auto &[it, ok] = configuration->headerNameMapping.emplace(headerName, vector(1, hfOrCppMod)); !ok)
            {
                string alreadyAdded;
                string tried;

                const HfOrCppMod local = it->second[0];
                if (hfOrCppMod.type == FileType::HEADER_FILE)
                {
                    tried = "Header-File " + hfOrCppMod.data.node->filePath;
                    alreadyAdded = "Header-File " + local.data.node->filePath;
                }
                else
                {
                    tried = "CppMod " + hfOrCppMod.data.cppMod->node->filePath;
                    alreadyAdded = "CppMod " + local.data.cppMod->node->filePath;
                }

                printErrorMessage(FORMAT("Header logical name maps to multiple files.\nConfiguration: {}\n"
                                         "Logical name: {}\nExisting mapping: {}\nAttempted mapping: {}",
                                         configuration->name, headerName, alreadyAdded, tried));
            }
        }

        flat_hash_map<const Node *, FileType> tempNodesType;
        tempNodesType.merge(reqNodesType);
        tempNodesType.merge(useReqNodesType);

        for (const auto &[node, type] : tempNodesType)
        {
            if (const auto &[it, ok] = configuration->nodesType.emplace(node, type); !ok)
            {
                string str;
                if (type == FileType::HEADER_FILE)
                {
                    str = "Header-File ";
                }
                else if (type == FileType::HEADER_UNIT)
                {
                    str = "C++20-Header-Unit ";
                }
                else
                {
                    str = "C++20-Module ";
                }
                printErrorMessage(FORMAT("File has conflicting classifications in the configuration.\n"
                                         "Configuration: {}\nPath: {}\nAttempted type: {}",
                                         configuration->name, node->filePath, str));
            }
        }
    }

    FOR_REQ_OBJECT_FILE_PRODUCERS(this, producer, dependency)
    {
        if (!dependency.isOpDependency() || !producer->isCppTarget)
        {
            continue;
        }
        auto *t = static_cast<CppTarget *>(producer);
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
                printErrorMessage(FORMAT("Module name is provided more than once.\nTarget: {}\nModule name: {}\n"
                                         "Module file: {}\nDependency target: {}",
                                         name, p.first, p.second->node->filePath, t->name));
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
                printErrorMessage(FORMAT("Logical name is defined as both a module and a header.\nTarget: {}\n"
                                         "Logical name: {}\nModule file: {}",
                                         name, p.first, p.second->node->filePath));
            }
        }
    }
}

void CppTarget::emplaceInHeaderNameMapping(string_view headerName, HfOrCppMod hfOrCppMod, const bool addInReq)
{
    const auto &[it, ok] = (addInReq ? reqHeaderNameMapping : useReqHeaderNameMapping).emplace(headerName, hfOrCppMod);
    const HfOrCppMod local = it->second;

    if (ok)
    {
        return;
    }

    string alreadyAdded;
    string tried;
    if (hfOrCppMod.type == FileType::HEADER_FILE)
    {
        tried = "Header-File " + hfOrCppMod.data.node->filePath;
        alreadyAdded = "Header-File " + local.data.node->filePath;
    }
    else
    {
        tried = "Header-Unit " + hfOrCppMod.data.cppMod->node->filePath;
        alreadyAdded = "Header-Unit " + local.data.cppMod->node->filePath;
    }

    printErrorMessage(FORMAT("Header logical name maps to multiple files.\nTarget: {}\nMapping scope: {}\n"
                             "Logical name: {}\nExisting mapping: {}\nAttempted mapping: {}",
                             name, addInReq ? "private/required" : "interface", headerName, alreadyAdded, tried));
}

void CppTarget::emplaceInNodesType(const Node *node, FileType type, const bool addInReq)
{
    if (const auto &[it, ok] = (addInReq ? reqNodesType : useReqNodesType).emplace(node, type); !ok)
    {
        string str;
        if (it->second == FileType::HEADER_FILE)
        {
            str = "Header-File ";
        }
        else if (it->second == FileType::HEADER_UNIT)
        {
            str = "C++20-Header-Unit ";
        }
        else
        {
            str = "C++20-Module ";
        }
        printErrorMessage(FORMAT("File has conflicting classifications in the target.\nTarget: {}\nPath: {}\n"
                                 "Existing type: {}",
                                 name, node->filePath, str));
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
        printErrorMessage(FORMAT("Cannot convert an unknown header file to a header unit.\nTarget: {}\n"
                                 "Logical name: {}",
                                 name, includeName));
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
        printErrorMessage(FORMAT("Cannot convert an unknown header unit to a header file.\nTarget: {}\n"
                                 "Logical name: {}",
                                 name, includeName));
    }

    removeHeaderUnit(includeName, addInReq, addInUseReq);
    addHeaderFile(includeName, headerNode, addInReq, addInUseReq);
}

void CppTarget::removeHeaderFile(const string_view includeName, const bool addInReq, const bool addInUseReq)
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
            printErrorMessage(FORMAT("Cannot remove an unknown header file.\nTarget: {}\nLogical name: {}\n"
                                     "Mapping scope: private/required",
                                     name, *p));
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
            printErrorMessage(FORMAT("Cannot remove an unknown header file.\nTarget: {}\nLogical name: {}\n"
                                     "Mapping scope: interface",
                                     name, *p));
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
                printErrorMessage(FORMAT("Cannot remove an unknown header unit.\nTarget: {}\nLogical name: {}\n"
                                         "Mapping scope: private/required",
                                         name, *p));
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
                printErrorMessage(FORMAT("Cannot remove an unknown header unit.\nTarget: {}\nLogical name: {}\n"
                                         "Mapping scope: interface",
                                         name, *p));
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
            printErrorMessage(FORMAT("Cannot remove a header that is not part of the aggregate header unit.\n"
                                     "Target: {}\nAggregate header unit: {}\nLogical name: {}",
                                     name, bigHu->node->filePath, *p));
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
        printErrorMessage(FORMAT("Cannot remove a header unit that is not registered with the target.\nTarget: {}\n"
                                 "Header unit: {}\nLogical name: {}",
                                 name, headerNode->filePath, includeName));
    }

    if (addInReq)
    {
        if (const auto &it = reqHeaderNameMapping.find(*p); it == reqHeaderNameMapping.end())
        {
            printErrorMessage(FORMAT("Cannot remove an unknown header unit.\nTarget: {}\nLogical name: {}\n"
                                     "Mapping scope: private/required",
                                     name, *p));
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
            printErrorMessage(FORMAT("Cannot remove an unknown header unit.\nTarget: {}\nLogical name: {}\n"
                                     "Mapping scope: interface",
                                     name, *p));
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

void CppTarget::addHeaderFile(const string_view includeName, const Node *headerFile, const bool addInReq,
                              const bool addInUseReq)
{
    string *p = new string(includeName);
    lowerCaseOnWindows(p->data(), p->size());
    if (addInReq)
    {
        emplaceInHeaderNameMapping(*p, HfOrCppMod{const_cast<Node *>(headerFile), FileType::HEADER_FILE, isSystem},
                                   true);
        emplaceInNodesType(headerFile, FileType::HEADER_FILE, true);
    }

    if (addInUseReq)
    {
        emplaceInHeaderNameMapping(*p, HfOrCppMod{const_cast<Node *>(headerFile), FileType::HEADER_FILE, isSystem},
                                   false);
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
            emplaceInHeaderNameMapping(*p, HfOrCppMod{hu, FileType::HEADER_UNIT, false}, true);
            emplaceInNodesType(headerUnit, FileType::HEADER_FILE, true);
        }

        if (addInUseReq)
        {
            emplaceInHeaderNameMapping(*p, HfOrCppMod{hu, FileType::HEADER_UNIT, false}, false);
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
                printErrorMessage(FORMAT("Header unit was added more than once.\nTarget: {}\nHeader unit: {}", name,
                                         headerUnit->filePath));
            }
        }

        hu = huDeps.emplace_back(new CppMod(this, headerUnit, CppModType::HEADER_UNIT));

        if (addInReq)
        {
            emplaceInHeaderNameMapping(*p, HfOrCppMod{hu, FileType::HEADER_UNIT, false}, true);
            emplaceInNodesType(headerUnit, FileType::HEADER_UNIT, true);
            hu->isReqHu = true;
        }

        if (addInUseReq)
        {
            emplaceInHeaderNameMapping(*p, HfOrCppMod{hu, FileType::HEADER_UNIT, false}, false);
            emplaceInNodesType(headerUnit, FileType::HEADER_UNIT, false);
            hu->isUseReqHu = true;
        }

        checkSameHeaderNameMapping(*p);
        hu->logicalName = *p;
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
            printErrorMessage(FORMAT("Composing-header logical name is duplicated.\nTarget: {}\nHeader unit: {}\n"
                                     "Logical name: {}",
                                     hu.target->name, hu.node->filePath, *logicalName));
        }

        if (hu.isReqHu)
        {
            emplaceInHeaderNameMapping(*logicalName, HfOrCppMod(&hu, FileType::HEADER_UNIT, true), true);
        }

        if (hu.isUseReqHu)
        {
            emplaceInHeaderNameMapping(*logicalName, HfOrCppMod(&hu, FileType::HEADER_UNIT, true), false);
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
                        printErrorMessage(FORMAT("Include directory was added more than once.\nTarget: {}\n"
                                                 "Directory: {}\nScope: private/required",
                                                 name, include->filePath));
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
                        printErrorMessage(FORMAT("Include directory was added more than once.\nTarget: {}\n"
                                                 "Directory: {}\nScope: interface",
                                                 name, include->filePath));
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
    hasObjectFiles = !srcFileDeps.empty() || !modFileDeps.empty() || !imodFileDeps.empty() ||
                     !adaptiveSourceNodes.empty() || !prebuiltObjects.empty();
    ObjectFileProducer::completeRoundOne();

    if constexpr (bsMode == BSMode::BUILD)
    {
        populateTransitiveProperties();
        setCommandHashes();
        connectBeforeTarget();
        return;
    }

    writeBigHeaderUnits();
    populateNameMappingsAndNodesType();
    if (configuration->evaluate(UseConfigurationScope::NO))
    {
        setHeaderFileStatusChanged(false);
    }
}

void CppTarget::writeConfigCacheAtConfigTime(string &buffer)
{
    ObjectFileProducer::writeConfigCacheAtConfigTime(buffer);

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

    writeUint32(buffer, adaptiveSourceNodes.size());
    for (const Node *node : adaptiveSourceNodes)
    {
        writeNode(buffer, node);
    }

    writeUint32(buffer, adaptiveGroupStarts.size());
    for (const uint32_t start : adaptiveGroupStarts)
    {
        writeUint32(buffer, start);
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

    writeBool(buffer, beforeTarget != nullptr);
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
            uint64_t bytesRead = 1; // (1 headerStatusChanged)
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

        uint64_t bytesRead = 1; // (1 headerStatusChanged)
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
    uint64_t bytesRead = configCacheRead;

    RealBTarget &rb = realBTargets[0];

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

    const uint32_t adaptiveSize = readUint32(ptr, bytesRead);
    adaptiveSourceNodes.reserve(adaptiveSize);
    for (uint32_t i = 0; i < adaptiveSize; ++i)
    {
        adaptiveSourceNodes.emplace_back(readHalfNode(ptr, bytesRead));
    }

    const uint32_t adaptiveGroupCount = readUint32(ptr, bytesRead);
    adaptiveGroupStarts.reserve(adaptiveGroupCount);
    for (uint32_t i = 0; i < adaptiveGroupCount; ++i)
    {
        adaptiveGroupStarts.emplace_back(readUint32(ptr, bytesRead));
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
        readInclDirsAtBuildTime(ptr, bytesRead, reqIncls);
        readInclDirsAtBuildTime(ptr, bytesRead, useReqIncls);
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
                    !reqHeaderNameMapping.emplace(name, HfOrCppMod{node, FileType::HEADER_FILE, isSystem}).second)
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
            const auto &[it, ok] = configuration->headerNameMapping.emplace(name, vector<HfOrCppMod>{});
            it->second.emplace_back(cacheIndex, node, FileType::HEADER_FILE, isSystem);
        }
    }

    if (readBool(ptr, bytesRead))
    {
        getOrCreateBeforeTarget();
    }

    if (bytesRead != configCache.size())
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }
    configCacheRead = bytesRead;
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
    if (addCppSource == AddCppSource::NO)
    {
        return;
    }

    if (configuration->evaluate(IsCppMod::NO))
    {
        assignToCppSrcs = true;
    }

    if constexpr (bsMode == BSMode::BUILD)
    {
        printErrorMessage(FORMAT("Source-directory expansion is only valid during configuration.\nTarget: {}\n"
                                 "Build mode: BUILD",
                                 name));
    }

    auto addNewFile = [&](const auto &k) {
        if (k.is_regular_file() && regex_match(k.path().filename().string(), std::regex(regexStr)))
        {
            const Node *node = Node::getNode(k);
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
        printErrorMessage(FORMAT("Source directory does not exist.\nTarget: {}\nDirectory: {}", name, s));
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

BTarget &CppTarget::getCppSrc(const string &str)
{
    Node *node = Node::getNodeNonNormalized(str, true);
    if (const auto source = std::ranges::find(srcFileDeps, node, [](const CppSrc *cppSrc) { return cppSrc->node; });
        source != srcFileDeps.end())
    {
        return **source;
    }
    if (std::ranges::find(adaptiveSourceNodes, node) != adaptiveSourceNodes.end())
    {
        return getOrCreateAdaptiveManager();
    }
    printErrorMessage(FORMAT("Source file is not registered with the target.\nTarget: {}\nSource file: {}", name, str));
    std::unreachable();
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
    printErrorMessage(
        FORMAT("Module interface is not registered with the target.\nTarget: {}\nModule file: {}", name, str));
    std::unreachable();
}

BTarget &CppTarget::getCppModule(const string &str)
{
    Node *node = Node::getNodeNonNormalized(str, true);
    if (const auto module = std::ranges::find(modFileDeps, node, [](const CppMod *cppMod) { return cppMod->node; });
        module != modFileDeps.end())
    {
        return **module;
    }
    if (std::ranges::find(adaptiveSourceNodes, node) != adaptiveSourceNodes.end())
    {
        return getOrCreateAdaptiveManager();
    }
    printErrorMessage(
        FORMAT("Module implementation is not registered with the target.\nTarget: {}\nModule file: {}", name, str));
    std::unreachable();
}

CppTarget &CppTarget::makeJumboToNormal(const NodeOrStr source)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *node = source.resolve(true);
        const JumboBuild previousJumboBuild = jumboBuild;
        jumboBuild = JumboBuild::NO;
        if (configuration->evaluate(IsCppMod::YES))
        {
            removeModuleFile(node);
            actuallyAddModuleFileConfigTime(node, "");
        }
        else
        {
            removeSourceFile(node);
            actuallyAddSourceFileConfigTime(node);
        }
        jumboBuild = previousJumboBuild;
    }
    return *this;
}

CppTarget &CppTarget::makeNormalToJumbo(const NodeOrStr source)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *node = source.resolve(true);
        if (sourceTypeOf(node->filePath) != SourceType::CPP)
        {
            printErrorMessage(FORMAT("Source does not have an eligible C++ extension.\nTarget: {}\nSource: {}", name,
                                     node->filePath));
        }

        const JumboBuild previousJumboBuild = jumboBuild;
        jumboBuild = JumboBuild::YES;
        if (configuration->evaluate(IsCppMod::YES))
        {
            removeModuleFile(node);
            actuallyAddModuleFileConfigTime(node, "");
        }
        else
        {
            removeSourceFile(node);
            actuallyAddSourceFileConfigTime(node);
        }
        jumboBuild = previousJumboBuild;
    }
    return *this;
}

CppTarget &CppTarget::removeSourceFile(const NodeOrStr source)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *node = source.resolve(true);
        if (const auto it = std::ranges::find(srcFileDeps, node, [](const CppSrc *unit) { return unit->node; });
            it != srcFileDeps.end())
        {
            srcFileDeps.erase(it);
            return *this;
        }
        if (const auto it = std::ranges::find(adaptiveSourceNodes, node); it != adaptiveSourceNodes.end())
        {
            const uint32_t removedIndex = std::distance(adaptiveSourceNodes.begin(), it);
            adaptiveSourceNodes.erase(it);
            for (uint32_t &start : adaptiveGroupStarts)
            {
                start -= start > removedIndex;
            }
            std::erase_if(adaptiveGroupStarts,
                          [this](const uint32_t start) { return start == 0 || start >= adaptiveSourceNodes.size(); });
            adaptiveGroupStarts.erase(std::unique(adaptiveGroupStarts.begin(), adaptiveGroupStarts.end()),
                                      adaptiveGroupStarts.end());
            return *this;
        }
        printErrorMessage(
            FORMAT("Source file is not registered with the target.\nTarget: {}\nSource: {}", name, node->filePath));
    }
    return *this;
}

CppTarget &CppTarget::removeModuleFile(const NodeOrStr source)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        Node *node = source.resolve(true);
        if (const auto it = std::ranges::find(modFileDeps, node, [](const CppMod *unit) { return unit->node; });
            it != modFileDeps.end())
        {
            modFileDeps.erase(it);
            return *this;
        }
        if (const auto it = std::ranges::find(adaptiveSourceNodes, node); it != adaptiveSourceNodes.end())
        {
            if (useIPC)
            {
                removeHeaderFile(getAdaptiveIncludeName(node), true, false);
            }
            const uint32_t removedIndex = std::distance(adaptiveSourceNodes.begin(), it);
            adaptiveSourceNodes.erase(it);
            for (uint32_t &start : adaptiveGroupStarts)
            {
                start -= start > removedIndex;
            }
            std::erase_if(adaptiveGroupStarts,
                          [this](const uint32_t start) { return start == 0 || start >= adaptiveSourceNodes.size(); });
            adaptiveGroupStarts.erase(std::unique(adaptiveGroupStarts.begin(), adaptiveGroupStarts.end()),
                                      adaptiveGroupStarts.end());
            return *this;
        }
        printErrorMessage(FORMAT("Module implementation is not registered with the target.\nTarget: {}\nSource: {}",
                                 name, node->filePath));
    }
    return *this;
}

CppMod &CppTarget::getCppHeaderUnit(const string &str, const bool addInReq, const bool addInUseReq)
{
    string includeName = str;
    lowerCaseOnWindows(includeName.data(), includeName.size());
    if (addInReq)
    {
        if (const auto it = reqHeaderNameMapping.find(includeName);
            it != reqHeaderNameMapping.end() && it->second.type == FileType::HEADER_UNIT)
        {
            return *it->second.data.cppMod;
        }
    }

    if (addInUseReq)
    {
        if (const auto it = useReqHeaderNameMapping.find(includeName);
            it != useReqHeaderNameMapping.end() && it->second.type == FileType::HEADER_UNIT)
        {
            return *it->second.data.cppMod;
        }
    }
    printErrorMessage(FORMAT("Header unit is not registered with the target.\nTarget: {}\nHeader unit: {}", name, str));
    std::unreachable();
}

string CppTarget::escapeAndQuoteDefineValue(string_view val)
{
    if (val.empty())
        return {};

    constexpr auto isSpecial = [](char c) noexcept {
        switch (c)
        {
        case ' ':
        case '\t':
        case '(':
        case ')':
        case ',':
        case '\'':
        case '"':
        case '\\':
        case '$':
        case '`':
        case '<':
        case '>':
        case '|':
        case '&':
        case ';':
            return true;
        default:
            return false;
        }
    };

    if (std::ranges::none_of(val, isSpecial))
        return string(val);

    string result;
    result.reserve(val.size() + 8); // +2 quotes, headroom for a few escapes
    result.push_back('"');
    for (const char c : val)
    {
        if (c == '"' || c == '\\' || c == '$' || c == '`')
            result.push_back('\\');
        result.push_back(c);
    }
    result.push_back('"');
    return result;
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

    // Keep include directories in deterministic specification/propagation order. A set would remove duplicates but
    // would also make the generated command's order dependent on its container ordering.

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
    FOR_REQ_OBJECT_FILE_PRODUCERS(this, producer, dependency)
    {
        if (!dependency.isOpDependency() || !producer->isCppTarget)
        {
            continue;
        }
        const auto *cppTarget = static_cast<const CppTarget *>(producer);
        deps += cppTarget->name + '\n';
    }
    return deps;
}

void CppTarget::verifyConfigCache(const string_view configCache) const
{
    uint64_t bytesRead = 0;
    verifyObjectFileProducerConfigCache(configCache, bytesRead);

    const uint32_t cachedSrcFileDepsSize = readUint32(configCache.data(), bytesRead);
    if (srcFileDeps.size() != cachedSrcFileDepsSize)
    {
        printErrorMessage(FORMAT("Configuration cache verification failed: source-file count mismatch.\nTarget: "
                                 "{}\nCurrent count: {}\nCached count: {}",
                                 getPrintName(), srcFileDeps.size(), cachedSrcFileDepsSize));
    }

    for (uint32_t i = 0; i < cachedSrcFileDepsSize; ++i)
    {
        const Node *cachedNode = readHalfNode(configCache.data(), bytesRead);
        if (i < srcFileDeps.size() && srcFileDeps[i]->node != cachedNode)
        {
            printErrorMessage(FORMAT("Configuration cache verification failed: source-file path mismatch.\nTarget: "
                                     "{}\nSource position: {}\nCurrent path: {}\nCached path: {}",
                                     getPrintName(), i,
                                     srcFileDeps[i]->node ? srcFileDeps[i]->node->filePath : "<null>",
                                     cachedNode ? cachedNode->filePath : "<null>"));
        }
    }

    const uint32_t cachedModFileDepsSize = readUint32(configCache.data(), bytesRead);
    if (modFileDeps.size() != cachedModFileDepsSize)
    {
        printErrorMessage(FORMAT("Configuration cache verification failed: module implementation count "
                                 "mismatch.\nTarget: {}\nCurrent count: {}\nCached count: {}",
                                 getPrintName(), modFileDeps.size(), cachedModFileDepsSize));
    }

    for (uint32_t i = 0; i < cachedModFileDepsSize; ++i)
    {
        const Node *cachedNode = readHalfNode(configCache.data(), bytesRead);
        if (i < modFileDeps.size() && modFileDeps[i]->node != cachedNode)
        {
            printErrorMessage(FORMAT("Configuration cache verification failed: module implementation path "
                                     "mismatch.\nTarget: {}\nModule position: {}\nCurrent path: {}\nCached path: {}",
                                     getPrintName(), i,
                                     modFileDeps[i]->node ? modFileDeps[i]->node->filePath : "<null>",
                                     cachedNode ? cachedNode->filePath : "<null>"));
        }
    }

    const uint32_t cachedAdaptiveSourceNodesSize = readUint32(configCache.data(), bytesRead);
    if (adaptiveSourceNodes.size() != cachedAdaptiveSourceNodesSize)
    {
        printErrorMessage(FORMAT("Configuration cache verification failed: adaptive-source count mismatch.\nTarget: "
                                 "{}\nCurrent count: {}\nCached count: {}",
                                 getPrintName(), adaptiveSourceNodes.size(), cachedAdaptiveSourceNodesSize));
    }

    for (uint32_t i = 0; i < cachedAdaptiveSourceNodesSize; ++i)
    {
        const Node *cachedNode = readHalfNode(configCache.data(), bytesRead);
        if (i < adaptiveSourceNodes.size() && adaptiveSourceNodes[i] != cachedNode)
        {
            printErrorMessage(FORMAT("Configuration cache verification failed: adaptive-source path mismatch.\n"
                                     "Target: {}\nSource position: {}\nCurrent path: {}\nCached path: {}",
                                     getPrintName(), i,
                                     adaptiveSourceNodes[i] ? adaptiveSourceNodes[i]->filePath : "<null>",
                                     cachedNode ? cachedNode->filePath : "<null>"));
        }
    }

    const uint32_t cachedAdaptiveGroupCount = readUint32(configCache.data(), bytesRead);
    if (adaptiveGroupStarts.size() != cachedAdaptiveGroupCount)
    {
        printErrorMessage(FORMAT("Configuration cache verification failed: adaptive-group count mismatch.\nTarget: "
                                 "{}\nCurrent count: {}\nCached count: {}",
                                 getPrintName(), adaptiveGroupStarts.size(), cachedAdaptiveGroupCount));
    }
    for (uint32_t i = 0; i < cachedAdaptiveGroupCount; ++i)
    {
        const uint32_t cachedStart = readUint32(configCache.data(), bytesRead);
        if (i < adaptiveGroupStarts.size() && adaptiveGroupStarts[i] != cachedStart)
        {
            printErrorMessage(FORMAT("Configuration cache verification failed: adaptive-group boundary mismatch.\n"
                                     "Target: {}\nBoundary position: {}\nCurrent index: {}\nCached index: {}",
                                     getPrintName(), i, adaptiveGroupStarts[i], cachedStart));
        }
    }

    const uint32_t cachedImodFileDepsSize = readUint32(configCache.data(), bytesRead);
    if (imodFileDeps.size() != cachedImodFileDepsSize)
    {
        printErrorMessage(FORMAT("Configuration cache verification failed: module interface count mismatch.\nTarget: "
                                 "{}\nCurrent count: {}\nCached count: {}",
                                 getPrintName(), imodFileDeps.size(), cachedImodFileDepsSize));
    }

    for (uint32_t i = 0; i < cachedImodFileDepsSize; ++i)
    {
        const Node *cachedNode = readHalfNode(configCache.data(), bytesRead);
        if (i < imodFileDeps.size() && imodFileDeps[i]->node != cachedNode)
        {
            printErrorMessage(FORMAT("Configuration cache verification failed: module interface path "
                                     "mismatch.\nTarget: {}\nModule position: {}\nCurrent path: {}\nCached path: {}",
                                     getPrintName(), i,
                                     imodFileDeps[i]->node ? imodFileDeps[i]->node->filePath : "<null>",
                                     cachedNode ? cachedNode->filePath : "<null>"));
        }

        const bool cachedIsPrimaryExport = readBool(configCache.data(), bytesRead);
        if (i < imodFileDeps.size() && (imodFileDeps[i]->type == CppModType::PRIMARY_EXPORT) != cachedIsPrimaryExport)
        {
            printErrorMessage(FORMAT("Configuration cache verification failed: primary-export flag mismatch.\nTarget: "
                                     "{}\nModule position: {}\nCurrent value: {}\nCached value: {}",
                                     getPrintName(), i, imodFileDeps[i]->type == CppModType::PRIMARY_EXPORT,
                                     cachedIsPrimaryExport));
        }
    }

    const uint32_t cachedHuDepsSize = readUint32(configCache.data(), bytesRead);
    if (huDeps.size() != cachedHuDepsSize)
    {
        printErrorMessage(FORMAT("Configuration cache verification failed: header-unit count mismatch.\nTarget: "
                                 "{}\nCurrent count: {}\nCached count: {}",
                                 getPrintName(), huDeps.size(), cachedHuDepsSize));
    }

    for (uint32_t i = 0; i < cachedHuDepsSize; ++i)
    {
        const Node *cachedNode = readHalfNode(configCache.data(), bytesRead);
        if (i < huDeps.size() && huDeps[i]->node != cachedNode)
        {
            printErrorMessage(FORMAT("Configuration cache verification failed: header-unit path mismatch.\nTarget: "
                                     "{}\nHeader-unit position: {}\nCurrent path: {}\nCached path: {}",
                                     getPrintName(), i, huDeps[i]->node ? huDeps[i]->node->filePath : "<null>",
                                     cachedNode ? cachedNode->filePath : "<null>"));
        }
    }

    const Node *cachedMyBuildDir = readHalfNode(configCache.data(), bytesRead);
    if (myBuildDir != cachedMyBuildDir)
    {
        printErrorMessage(FORMAT("Configuration cache verification failed: build directory mismatch.\nTarget: "
                                 "{}\nCurrent path: {}\nCached path: {}",
                                 getPrintName(), myBuildDir ? myBuildDir->filePath : "<null>",
                                 cachedMyBuildDir ? cachedMyBuildDir->filePath : "<null>"));
    }

    if (configuration->evaluate(IsCppMod::NO) || !useIPC)
    {
        const uint32_t cachedReqInclsSize = readUint32(configCache.data(), bytesRead);
        if (reqIncls.size() != cachedReqInclsSize)
        {
            printErrorMessage(FORMAT("Configuration cache verification failed: private include count "
                                     "mismatch.\nTarget: {}\nCurrent count: {}\nCached count: {}",
                                     getPrintName(), reqIncls.size(), cachedReqInclsSize));
        }

        for (uint32_t i = 0; i < cachedReqInclsSize; ++i)
        {
            const Node *cachedNode = readHalfNode(configCache.data(), bytesRead);
            const bool cachedIsStandard = readBool(configCache.data(), bytesRead);
            if (i < reqIncls.size() && reqIncls[i].node != cachedNode)
            {
                printErrorMessage(
                    FORMAT("Configuration cache verification failed: private include path mismatch.\nTarget: "
                           "{}\nInclude position: {}\nCurrent path: {}\nCached path: {}",
                           getPrintName(), i, reqIncls[i].node ? reqIncls[i].node->filePath : "<null>",
                           cachedNode ? cachedNode->filePath : "<null>"));
            }
            if (i < reqIncls.size() && reqIncls[i].isStandard != cachedIsStandard)
            {
                printErrorMessage(
                    FORMAT("Configuration cache verification failed: private include classification mismatch.\n"
                           "Target: {}\nInclude position: {}\nCurrent system classification: {}\n"
                           "Cached system classification: {}",
                           getPrintName(), i, reqIncls[i].isStandard, cachedIsStandard));
            }
        }

        const uint32_t cachedUseReqInclsSize = readUint32(configCache.data(), bytesRead);
        if (useReqIncls.size() != cachedUseReqInclsSize)
        {
            printErrorMessage(FORMAT("Configuration cache verification failed: interface include count "
                                     "mismatch.\nTarget: {}\nCurrent count: {}\nCached count: {}",
                                     getPrintName(), useReqIncls.size(), cachedUseReqInclsSize));
        }

        for (uint32_t i = 0; i < cachedUseReqInclsSize; ++i)
        {
            const Node *cachedNode = readHalfNode(configCache.data(), bytesRead);
            const bool cachedIsStandard = readBool(configCache.data(), bytesRead);
            if (i < useReqIncls.size() && useReqIncls[i].node != cachedNode)
            {
                printErrorMessage(
                    FORMAT("Configuration cache verification failed: interface include path mismatch.\nTarget: "
                           "{}\nInclude position: {}\nCurrent path: {}\nCached path: {}",
                           getPrintName(), i, useReqIncls[i].node ? useReqIncls[i].node->filePath : "<null>",
                           cachedNode ? cachedNode->filePath : "<null>"));
            }
            if (i < useReqIncls.size() && useReqIncls[i].isStandard != cachedIsStandard)
            {
                printErrorMessage(
                    FORMAT("Configuration cache verification failed: interface include classification mismatch.\n"
                           "Target: {}\nInclude position: {}\nCurrent system classification: {}\n"
                           "Cached system classification: {}",
                           getPrintName(), i, useReqIncls[i].isStandard, cachedIsStandard));
            }
        }
    }

    if (configuration->evaluate(IsCppMod::YES))
    {
        auto verifyHeaderNameMapping = [&](const flat_hash_map<string_view, HfOrCppMod> &mapping,
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
                    printErrorMessage(FORMAT("Configuration cache verification failed: cached logical name is "
                                             "missing.\nTarget: {}\nMapping: {}\nEntry position: {}\nLogical name: {}",
                                             getPrintName(), mapName, i, cachedName));
                }
                else if (it->second.data.node != cachedNode)
                {
                    printErrorMessage(
                        FORMAT("Configuration cache verification failed: logical-name path mismatch.\nTarget: "
                               "{}\nMapping: {}\nLogical name: {}\nCurrent path: {}\nCached path: {}",
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
                if (h.type == FileType::HEADER_FILE)
                {
                    ++currentNonUnitCount;
                }
            }
            if (currentNonUnitCount != cachedCount)
            {
                printErrorMessage(FORMAT("Configuration cache verification failed: non-unit header count "
                                         "mismatch.\nTarget: {}\nMapping: {}\nCurrent count: {}\nCached count: {}",
                                         getPrintName(), mapName, currentNonUnitCount, cachedCount));
            }
        };

        verifyHeaderNameMapping(reqHeaderNameMapping, "reqHeaderNameMapping");
        verifyHeaderNameMapping(useReqHeaderNameMapping, "useReqHeaderNameMapping");
    }

    const bool cachedHasBeforeTarget = readBool(configCache.data(), bytesRead);
    if (cachedHasBeforeTarget != (beforeTarget != nullptr))
    {
        printErrorMessage(FORMAT("Configuration cache verification failed: before-target presence mismatch.\n"
                                 "Target: {}\nCurrent value: {}\nCached value: {}",
                                 getPrintName(), beforeTarget != nullptr, cachedHasBeforeTarget));
    }
    if (configCache.size() != bytesRead)
    {
        printErrorMessage(FORMAT("Configuration cache verification failed: entry size mismatch.\nTarget: {}\nEntry "
                                 "size: {} bytes\nBytes consumed: {}",
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
        if (!ploat->rootObjectFileProducers.emplace(objectFileProducer).second)
        {
            printErrorMessage(FORMAT("An object-file producer was registered with a link target more than once.\n"
                                     "Link target: {}\nProducer: {}",
                                     ploat->getPrintName(), objectFileProducer->getPrintName()));
        }
        // PLOAT decides its round-zero object dependencies after producer round one has finalized hasObjectFiles.
        ploat->realBTargets[1].addDep<BTargetType::UNKNOWN>(&objectFileProducer->realBTargets[1]);
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
        if (ploat != nullptr && ploat->evaluate(TargetType::LIBRARY_SHARED))
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
            // Object-only and static targets have no DLL boundary, but source/header declarations still need the
            // API macro to exist.
            ptr->reqCompileDefinitions.emplace(Define(define, ""));
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

template CppTarget &CppTarget::moduleFiles<>(NodeOrStr);
template CppTarget &CppTarget::publicCompileDefines<>(const string &, const string &);
template CppTarget &CppTarget::privateCompileDefines<>(const string &, const string &);
template CppTarget &CppTarget::publicHUIncludes<>(NodeOrStr);
template CppTarget &CppTarget::privateHUIncludes<>(NodeOrStr);
template CppTarget &CppTarget::publicIncludesSource<>(NodeOrStr);
template CppTarget &CppTarget::publicSystemIncludesSource<>(NodeOrStr);
template CppTarget &CppTarget::privateIncludesSource<>(NodeOrStr);
template CppTarget &CppTarget::publicIncludes<>(NodeOrStr);
template CppTarget &CppTarget::privateIncludes<>(NodeOrStr);
template CppTarget &CppTarget::publicHUDirs<>(NodeOrStr, const string &);
template CppTarget &CppTarget::privateHUDirs<>(NodeOrStr, const string &);
template CppTarget &CppTarget::publicHUDirsRE<>(NodeOrStr, const string &, const string &);
template CppTarget &CppTarget::privateHUDirsRE<>(NodeOrStr, const string &, const string &);
template CppTarget &CppTarget::publicIncDirs<>(NodeOrStr, const string &);
template CppTarget &CppTarget::privateIncDirs<>(NodeOrStr, const string &);
template CppTarget &CppTarget::publicIncDirsRE<>(NodeOrStr, const string &, const string &);
template CppTarget &CppTarget::privateIncDirsRE<>(NodeOrStr, const string &, const string &);
