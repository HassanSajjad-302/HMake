
#include "CppMod.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "Configuration.hpp"
#include "CppTarget.hpp"
#include "IPCManagerCompiler.hpp"
#include "JConsts.hpp"
#include "rapidhash/rapidhash.h"

#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <memory_resource>
#include <utility>

using std::tie, std::ifstream, std::exception, std::lock_guard, P2978::IPCManagerBS;

bool CompareCppSrc::operator()(const CppSrc &lhs, const CppSrc &rhs) const
{
    return lhs.node < rhs.node;
}

bool CompareCppSrc::operator()(const Node *lhs, const CppSrc &rhs) const
{
    return lhs < rhs.node;
}

bool CompareCppSrc::operator()(const CppSrc &lhs, const Node *rhs) const
{
    return lhs.node < rhs;
}

CppSrc::CppSrc(CppTarget *target_, const Node *node_, CppModType cppModType)
    : ObjectFile(static_cast<uint64_t>(node_->myId) << 32 | static_cast<uint64_t>(target_->cacheIndex) << 3 |
                     static_cast<uint64_t>(cppModType),
                 cppModType == CppModType::CPP_SRC ? BTargetType::CPP_SRC : BTargetType::CPP_MOD, true, false),
      target(target_), node{node_}
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        return;
    }

    if (cppModType != CppModType::CPP_SRC)
    {
        return;
    }

    {
        // reading config-cache.

        uint32_t bytesRead = 0;
        const string_view configCache = fileTargetCaches[cacheIndex].configCache;
        objectNode = readHalfNode(configCache.data(), bytesRead);

        if (4 != configCache.size())
        {
            HMAKE_HMAKE_INTERNAL_ERROR
        }
    }

    uint32_t bytesRead = 0;

    const string_view buildCache = fileTargetCaches[cacheIndex].getBuildCache();
    const char *ptr = buildCache.data();

    const_cast<Node *>(node)->checkHashing = true;

    const uint32_t headerFilesSize = readUint32(ptr, bytesRead);

    cachedHeaderFiles = span{reinterpret_cast<const uint32_t *>(ptr + bytesRead), headerFilesSize};
    for (uint32_t i = 0; i < headerFilesSize; ++i)
    {
        Node *headerNode = readHalfNode(ptr, bytesRead);
        headerNode->checkHashing = true;
    }

    if (bytesRead != buildCache.size())
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }

    objectNode->toBeChecked = true;
}

string CppSrc::getPrintName() const
{
    return node->filePath;
}

void CppSrc::getCompileCommand(std::pmr::string &compileCommand) const
{
    const Compiler &compiler = target->configuration->compilerFeatures.compiler;
    if (sourceType == SourceType::CPP)
    {
        compileCommand = target->configuration->cppCompileCommand;
    }
    else if (sourceType == SourceType::C)
    {
        compileCommand = target->configuration->cCompileCommand;
    }
    else if (sourceType == SourceType::ASSEMBLY)
    {
        compileCommand = target->configuration->assemblyCompileCommand;
    }
    target->setCompileCommand(compileCommand);

    if (compiler.bTFamily == BTFamily::MSVC)
    {
        compileCommand += "-c /nologo /showIncludes /TP \"" + node->filePath + "\" /Fo\"" + objectNode->filePath + "\"";
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
        compileCommand += "-c -MMD \"" + node->filePath + "\" -o \"" + objectNode->filePath + "\"";
    }
}

// TODO
// currently un-used. should be used in ipc based builds?
bool CppSrc::ignoreHeaderFile(const string_view child) const
{
    return false;
    // It is assumed that both paths are normalized strings
    for (const InclNode &inclNode : target->reqIncls)
    {
        if (inclNode.isStandard)
        {
            if (childInParentPathNormalized(inclNode.node->filePath, child))
            {
                return true;
            }
        }
    }
    return false;
}

void CppSrc::parseDepsFromMSVCTextOutput(string &output, const bool isClang)
{
    const string includeFileNote = "Note: including file:";

    if (realBTargets[0].exitStatus != EXIT_SUCCESS)
    {
        string str = output;
        output.clear();
        uint32_t start = 0;
        for (uint64_t i = str.find('\n', start); i != string::npos; start = i + 1, i = str.find('\n', start))
        {
            if (string_view line = str.substr(start, i - start + 1); !line.contains(includeFileNote))
            {
                output += string(line);
            }
        }
        return;
    }

    uint64_t startPos = 0;
    uint64_t lineEnd;
    string_view line;

    constexpr uint32_t stackSize = 128 * 1024;
    char buffer[stackSize];

    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string treatedOutput(&alloc);
    treatedOutput.reserve(stackSize);

    if (!isClang)
    {
        // MSVC also prints the name of the file which is being skipped.

        lineEnd = output.find('\n');

        if (lineEnd == string::npos)
        {
            return;
        }

        line = {output.begin() + startPos, output.begin() + lineEnd + 1};

        startPos = lineEnd + 1;
    }

    if (output.size() == startPos)
    {
        output = std::move(treatedOutput);
        return;
    }

    lineEnd = output.find('\n', startPos);
    while (true)
    {

        line = string_view(output.begin() + startPos, output.begin() + lineEnd + 1);
        if (size_t pos = line.find(includeFileNote); pos != string::npos)
        {
            pos = line.find_first_not_of(' ', includeFileNote.size());

            if (line.size() >= pos + 1)
            {
                // Last character is \r for some reason with MSVC.
                const uint8_t sub = isClang ? 1 : 2;
                // MSVC compiler can output header-includes with / as path separator

                for (auto it = line.begin() + pos; it != line.end() - sub; ++it)
                {
                    if (*it == '/')
                    {
                        const_cast<char &>(*it) = '\\';
                    }
                }

                string_view headerView{line.begin() + pos, line.end() - sub};

                // TODO
                // If compile-command is all lower-cased, then this might not be needed
                // Some compilers can input same header-file twice, if that is the case, then we should first make
                // the array unique.
                lowerCaseOnWindows(const_cast<char *>(headerView.data()), headerView.size());
                if (!ignoreHeaderFile(headerView))
                {
                    if (Node *headerNode = Node::getHalfNode(headerView); !headerFiles.contains(headerNode))
                    {
                        headerFiles.emplace(headerNode);
                    }
                }
            }
            else
            {
                printErrorMessage(FORMAT("Empty Header Include {}\n", std::string(line)));
            }
        }
        else
        {
            treatedOutput.append(line);
        }

        startPos = lineEnd + 1;
        if (output.size() == startPos)
        {
            output = treatedOutput;
            break;
        }
        /*if (lineEnd > output.size() - 5)
        {
            bool breakpoint = true;
        }*/
        lineEnd = output.find('\n', startPos);
        if (lineEnd == -1)
        {
            bool breakpoint = true;
        }
    }
}

void CppSrc::parseDepsFromGCCDepsOutput(Builder &builder)
{
    string headerDepsFile = objectNode->filePath;
    // replacing .o ext with .d
    headerDepsFile[headerDepsFile.size() - 1] = 'd';

    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];

    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string headerFileDeps(&alloc);
    headerFileDeps.reserve(stackSize);
    fileToString(headerDepsFile, headerFileDeps);

    const vector<string_view> headerDeps = split(headerFileDeps, '\n');

    // The First 2 lines are skipped as these are .o and .cpp file.
    // If the file is preprocessed, it does not generate the extra line
    const auto endIt = headerDeps.end() - 1;

    if (headerDeps.size() > 2)
    {
        for (auto iter = headerDeps.begin() + 2; iter != endIt; ++iter)
        {
            const size_t pos = iter->find_first_not_of(" ");
            const auto it = iter->begin() + pos;
            if (const string_view headerView{&*it, iter->size() - (iter->ends_with('\\') ? 2 : 0) - pos};
                !ignoreHeaderFile(headerView))
            {
                headerFiles.emplace(Node::getHalfNode(headerView));
            }
        }
    }
}

void CppSrc::parseHeaderDeps(string &output, Builder &builder)
{
    if (target->configuration->compilerFeatures.compiler.bTFamily == BTFamily::MSVC)
    {
        parseDepsFromMSVCTextOutput(output,
                                    target->configuration->compilerFeatures.compiler.btSubFamily == BTSubFamily::CLANG);
    }
    else
    {
        // in-case of MSVC header-deps are parsed even in case of compilation failure to clean the std output.
        if (realBTargets[0].exitStatus == EXIT_SUCCESS)
        {
            parseDepsFromGCCDepsOutput(builder);
        }
    }
}

// An invariant is that paths are lexically normalized.
bool pathContainsFile(string_view dir, const string_view file)
{
    string_view withoutFileName(file.data(), file.find_last_of(slashc));

    if (dir.size() > withoutFileName.size())
    {
        return false;
    }

    // This stops checking when it reaches dir.end(), so it's OK if file
    // has more dir components afterward. They won't be checked.
    return std::equal(dir.begin(), dir.end(), withoutFileName.begin());
}

void CppSrc::setFileStatus()
{
    RealBTarget &rb = realBTargets[0];
    assert(rb.updateStatus == UpdateStatus::UNCHECKED);

    if (node->fileType == file_type::not_found)
    {
        printErrorMessage(FORMAT("Source-File {}\n of target {}\n not found.\n", node->filePath, target->name));
    }

    if (objectNode->fileType == file_type::not_found)
    {
        rb.updateStatus = UpdateStatus::UPDATED_NEEDED;
        return;
    }

    // pmr
    std::vector<uint64_t> contentHashes;
    contentHashes.reserve(1 + 1 + cachedHeaderFiles.size());
    contentHashes.emplace_back(commandHash);
    contentHashes.emplace_back(node->contentHash);
    for (const uint32_t nodeIndex : cachedHeaderFiles)
    {
        contentHashes.emplace_back(Node::getHalfNode(nodeIndex)->contentHash);
    }
    rb.cumulativeHash = rapidhash(contentHashes.data(), contentHashes.size() * 8);

    ObjectFile::setFileStatus();
}

bool CppSrc::isEventRegistered(Builder &builder)
{
    if (!selectiveBuild)
    {
        return false;
    }
    const RealBTarget &rb = realBTargets[0];
    if (rb.updateStatus == UpdateStatus::UNCHECKED)
    {
        setFileStatus();
    }
    if (rb.updateStatus != UpdateStatus::UPDATED_NEEDED)
    {
        return false;
    }

    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];

    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string cppFullCompileCommand(&alloc);
    cppFullCompileCommand.reserve(stackSize);
    getCompileCommand(cppFullCompileCommand);
    if (dryRun)
    {
        cppFullCompileCommand += '\n';
        printMessage(cppFullCompileCommand);
        return false;
    }

    realBTargets[0].launchTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    run.startAsyncProcess(cppFullCompileCommand.c_str(), builder, this, false);
    return true;
}

bool CppSrc::isEventCompleted(Builder &builder, string_view)
{
    parseHeaderDeps(*run.output, builder);

    if (realBTargets[0].exitStatus == EXIT_SUCCESS)
    {
        // maybe move to where these are parsed
        for (Node *headerNode : headerFiles)
        {
            headerNode->checkHashing = true;
        }
        buildCacheUpdated = true;
        buildFooterUpdated = true;
    }

    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];

    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string cppFullCompileCommand(&alloc);
    std::pmr::string &outputStr = cppFullCompileCommand;
    alloc.release();
    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::cyan);
    }

    if (run.output->empty())
    {
        outputStr += FORMAT("[{}/{}]C++Source {} {}\n", builder.updatedCount, builder.updateBTargetsSizeGoal,
                            node->filePath, target->name);
    }
    else
    {
        getCompileCommand(outputStr);
        outputStr.push_back('\n');
    }

    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::reset);
    }

    if (!run.output->empty())
    {
        outputStr += *run.output;
        outputStr.push_back('\n');
    }
    fwrite(outputStr.c_str(), 1, outputStr.size(), stdout);
    return false;
}

void CppSrc::writeConfigCacheAtConfigTime(string &buffer)
{
    const string fileNumber = toString(node->myId);
    objectNode =
        Node::getNode(target->myBuildDir->filePath + slashc + node->getFileName() + fileNumber + ".o", true, true);
    writeNode(buffer, objectNode);
}

void CppSrc::writeBuildCacheAtConfigTime(string &buffer)
{
    // sizeof header-files
    writeUint32(buffer, 0);
}

void CppSrc::writeBuildCacheAtBuildTime(string &buffer)
{
    RealBTarget &rb = realBTargets[0];
    // pmr
    std::vector<uint64_t> contentHashes;
    contentHashes.reserve(1 + 1 + headerFiles.size()); // headerFiles, not cachedHeaderFiles
    contentHashes.emplace_back(commandHash);
    contentHashes.emplace_back(node->contentHash);
    for (const Node *headerNode : headerFiles) // headerFiles, not cachedHeaderFiles
    {
        if (headerNode->lastWriteTime > rb.launchTime)
        {
            // File was modified after process launched — hash is stale.
            contentHashes.emplace_back(0);
        }
        else
        {
            contentHashes.emplace_back(headerNode->contentHash);
        }
    }
    rb.cumulativeHash = rapidhash(contentHashes.data(), contentHashes.size() * 8);
    writeUint32(buffer, headerFiles.size());
    for (const Node *header : headerFiles)
    {
        writeNode(buffer, header);
    }
}

void CppSrc::verifyBuildCache(const string_view buildCache) const
{
    const RealBTarget &rb = realBTargets[0];

    if constexpr (bsMode == BSMode::BUILD)
    {
        // Recompute cumulativeHash and dump to debug file for comparison.
        std::vector<uint64_t> contentHashes;
        contentHashes.reserve(1 + 1 + headerFiles.size());
        contentHashes.emplace_back(commandHash);
        contentHashes.emplace_back(node->contentHash);
        for (const Node *headerNode : headerFiles)
        {
            if (headerNode->lastWriteTime > rb.launchTime)
            {
                contentHashes.emplace_back(0);
            }
            else
            {
                contentHashes.emplace_back(headerNode->contentHash);
            }
        }

        {
            const uint64_t recomputedHash = rapidhash(contentHashes.data(), contentHashes.size() * 8);
            const path debugFile =
                target->myBuildDir->filePath + slashc + string("hashes") + toString(node->myId) + ".txt";
            if (std::ofstream out(debugFile, std::ios::app); out)
            {
                out << "commandHash:       " << commandHash << '\n';
                out << "node->contentHash: " << node->contentHash << '\n';
                uint32_t i = 0;
                for (const Node *headerNode : headerFiles)
                {
                    out << "header[" << i++ << "] " << (headerNode ? headerNode->filePath : "<null>")
                        << " hash=" << contentHashes[i + 1] << '\n'; // +2 offset: [0]=commandHash [1]=node->contentHash
                }
                out << "recomputedHash: " << recomputedHash << '\n';
                out << "storedHash:     " << rb.cumulativeHash << '\n';
            }

            if (recomputedHash != rb.cumulativeHash)
            {
                printErrorMessage(FORMAT("{} caching-verification failed as recomputed cumulativeHash != stored\n"
                                         "recomputed={} stored={}\n",
                                         getPrintName(), recomputedHash, rb.cumulativeHash));
            }
        }
    }

    uint32_t bytesRead = 0;

    const uint32_t cachedHeaderFilesSize = readUint32(buildCache.data(), bytesRead);
    if (headerFiles.size() != cachedHeaderFilesSize)
    {
        printErrorMessage(FORMAT("{} caching-verification failed as headerFiles.size() != cached headerFilesSize\n"
                                 "current={} cached={}\n",
                                 getPrintName(), headerFiles.size(), cachedHeaderFilesSize));
    }

    for (uint32_t i = 0; i < cachedHeaderFilesSize; ++i)
    {
        const Node *cachedNode = readHalfNode(buildCache.data(), bytesRead);
        if (!headerFiles.contains(cachedNode))
        {
            printErrorMessage(FORMAT("{} caching-verification failed as cached headerFile\n{} at index {}\n"
                                     "is not present in current headerFiles\n",
                                     getPrintName(), cachedNode ? cachedNode->filePath : "<null>", i));
        }
    }

    verifyBTargetHeader(buildCache, bytesRead);
    if (buildCache.size() != bytesRead)
    {
        printErrorMessage(FORMAT("{} caching-verification failed as buildCache.size() != bytesRead {} vs {}\n",
                                 getPrintName(), buildCache.size(), bytesRead));
    }
}

void CppSrc::verifyConfigCache(const string_view configCache) const
{
    ObjectFile::verifyConfigCache(configCache);
}

bool operator<(const CppSrc &lhs, const CppSrc &rhs)
{
    return lhs.node < rhs.node;
}

CppMod::CppMod(CppTarget *target_, const Node *node_, const CppModType cppModType)
    : CppSrc(target_, node_, cppModType), type(cppModType)

{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        return;
    }
    RealBTarget &rb = realBTargets[0];

    const bool isHU = type == CppModType::HEADER_UNIT;
    const bool isImpl = type == CppModType::PRIMARY_IMPLEMENTATION;

    {
        uint32_t bytesRead = 0;
        const string_view configCache = fileTargetCaches[cacheIndex].configCache;
        const char *ptr = configCache.data();

        if (!isImpl)
        {
            interfaceNode = readHalfNode(ptr, bytesRead);
            if (!isHU)
            {
                logicalNames.emplace_back(readStringView(ptr, bytesRead));
            }
        }

        if (!isHU)
        {
            objectNode = readHalfNode(ptr, bytesRead);

            const uint32_t logicalNamesSize = readUint32(ptr, bytesRead);
            logicalNames.reserve(logicalNamesSize);

            for (uint32_t j = 0; j < logicalNamesSize; ++j)
            {
                string_view str = readStringView(ptr, bytesRead);
                target->imodNames.emplace(str, this);
            }
        }
        else
        {
            isReqHu = readBool(ptr, bytesRead);
            isUseReqHu = readBool(ptr, bytesRead);

            const uint32_t headerFileModuleSize = readUint32(ptr, bytesRead);
            const uint32_t logicalNamesSize = readUint32(ptr, bytesRead);
            logicalNames.reserve(logicalNamesSize + headerFileModuleSize);

            for (uint32_t j = 0; j < headerFileModuleSize; ++j)
            {
                string_view headerFileName = readStringView(ptr, bytesRead);
                if (target->useIPC)
                {
                    Node *headerNode = readHalfNode(ptr, bytesRead);
                    composingHeaders.emplace(headerFileName, headerNode);
                }
                else
                {
                    composingHeaders.emplace(headerFileName, nullptr);
                }
                logicalNames.emplace_back(headerFileName);

                if (isReqHu)
                {
                    target->reqHeaderNameMapping.emplace(headerFileName, HeaderFileOrUnit(this, target->isSystem));
                }

                if (isUseReqHu)
                {
                    const auto &[it, ok] =
                        target->configuration->headerNameMapping.emplace(headerFileName, vector<HeaderFileOrUnit>{});
                    it->second.emplace_back(target->cacheIndex, this, target->isSystem);
                }
            }

            for (uint32_t j = 0; j < logicalNamesSize; ++j)
            {
                string_view str = readStringView(ptr, bytesRead);
                logicalNames.emplace_back(str);
                if (isReqHu)
                {
                    target->reqHeaderNameMapping.emplace(str, HeaderFileOrUnit(this, target->isSystem));
                }

                if (isUseReqHu)
                {
                    const auto &[it, ok] =
                        target->configuration->headerNameMapping.emplace(str, vector<HeaderFileOrUnit>{});
                    it->second.emplace_back(target->cacheIndex, this, target->isSystem);
                }
            }
        }

        if (bytesRead != configCache.size())
        {
            HMAKE_HMAKE_INTERNAL_ERROR
        }
    }

    uint32_t bytesRead = 0;

    const string_view buildCache = fileTargetCaches[cacheIndex].getBuildCache();
    const char *ptr = buildCache.data();

    // headerStatusChanged
    if (readBool(ptr, bytesRead))
    {
        rb.updateStatus = UpdateStatus::UPDATED_NEEDED;
    }

    const_cast<Node *>(node)->checkHashing = true;

    const uint32_t headerFilesSize = readUint32(ptr, bytesRead);

    cachedHeaderFiles = span{reinterpret_cast<const uint32_t *>(ptr + bytesRead), headerFilesSize};
    for (uint32_t i = 0; i < headerFilesSize; ++i)
    {
        Node *headerNode = readHalfNode(ptr, bytesRead);
        headerNode->checkHashing = true;
    }

    const uint32_t cachedDepsSize = readUint32(ptr, bytesRead);
    cachedDeps = span{reinterpret_cast<const uint32_t *>(ptr + bytesRead), cachedDepsSize};

    bytesRead += 4 * cachedDepsSize;

    if (bytesRead != buildCache.size())
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }

    if (!isImpl)
    {
        interfaceNode->toBeChecked = true;
    }

    if (!isHU)
    {
        objectNode->toBeChecked = true;
    }
}

void CppMod::makeMemoryFileMapping()
{
    if (memoryMappingCompleted)
    {
        return;
    }

    P2978::BMIFile file;
    file.filePath = interfaceNode->filePath;
    if (const auto &r = IPCManagerBS::createSharedMemoryBMIFile(file); !r)
    {
        printErrorMessage(FORMAT("Could not make mapping for the file {}\n of target {}\n. Error {}\n", node->filePath,
                                 target->name, r.error()));
    }
    interfaceFileSize = file.fileSize;
    memoryMappingCompleted = true;
}

void CppMod::populateAllDeps()
{
    if (isAllDepsPopulated)
    {
        return;
    }

    isAllDepsPopulated = true;

    if (realBTargets[0].updateStatus == UpdateStatus::UPDATED_NEEDED)
    {
        for (const CppModWithDirect &depWithDirect : allCppModDeps)
        {
            if (CppMod *cppMod = depWithDirect.getPointer();
                allCppModDeps.emplace(CppModWithDirect(cppMod, false)).second)
            {
                cppMod->populateAllDeps();
                for (const CppModWithDirect &transitive : cppMod->allCppModDeps)
                {
                    allCppModDeps.emplace(transitive.getPointer(), false);
                }
            }
        }
    }
    else
    {
        for (const uint32_t &dep : cachedDeps)
        {
            if (CppMod *cppMod = static_cast<CppMod *>(fileTargetCaches[dep].bTarget);
                allCppModDeps.emplace(CppModWithDirect(cppMod, false)).second)
            {
                cppMod->populateAllDeps();
                for (const CppModWithDirect &transitive : cppMod->allCppModDeps)
                {

                    allCppModDeps.emplace(transitive.getPointer(), false);
                }
            }
        }
    }
}

void CppMod::makeAndSendBTCModule(CppMod &mod)
{
    realBTargets[0].launchTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    // todo
    // write this buffer directly.

    mod.makeMemoryFileMapping();
    mod.populateAllDeps();

    P2978::BTCModule btcModule;
    btcModule.requested.filePath = mod.interfaceNode->filePath;
    btcModule.requested.fileSize = mod.interfaceFileSize;
    btcModule.isSystem = mod.target->isSystem;

    for (const CppModWithDirect &transitive : mod.allCppModDeps)
    {
        CppMod *modDep = transitive.getPointer();
        if (!allCppModDeps.emplace(modDep, false).second)
        {
            continue;
        }

        modDep->makeMemoryFileMapping();

        P2978::ModuleDep dep;
        dep.isHeaderUnit = modDep->type == CppModType::HEADER_UNIT;
        dep.file.filePath = modDep->interfaceNode->filePath;
        dep.file.fileSize = modDep->interfaceFileSize;
        dep.isSystem = modDep->target->isSystem;
        dep.logicalNames.assign(modDep->logicalNames.begin(), modDep->logicalNames.end());

        btcModule.modDeps.emplace_back(std::move(dep));
    }

    if (const auto &r2 = ipcManager->sendMessage(btcModule); !r2)
    {
        printErrorMessage(FORMAT("send-message fail of module-file {}\n for module-file {}\n of target {}\n.",
                                 mod.node->filePath, node->filePath, target->name));
    }
}

// For debugging purposes
P2978::BTCNonModule deserializeBTCNonModule(std::string_view buffer)
{
    P2978::BTCNonModule result;
    const char *ptr = buffer.data();
    uint32_t bytesRead = 0;

    // BTCNonModule::isHeaderUnit
    result.isHeaderUnit = readBool(ptr, bytesRead);

    // BTCNonModule::isSystem
    result.isSystem = readBool(ptr, bytesRead);

    // BTCNonModule::headerFiles
    uint32_t headerFilesCount = readUint32(ptr, bytesRead);
    result.headerFiles.reserve(headerFilesCount);
    for (uint32_t i = 0; i < headerFilesCount; ++i)
    {
        P2978::HeaderFile hf;

        // HeaderFile::logicalName
        std::string_view logicalNameView = readStringView(ptr, bytesRead);
        hf.logicalName = logicalNameView;

        // HeaderFile::filePath
        std::string_view filePathView = readStringView(ptr, bytesRead);
        hf.filePath = filePathView;

        // HeaderFile::isSystem
        hf.isSystem = readBool(ptr, bytesRead);

        result.headerFiles.emplace_back(hf);
    }

    // BTCNonModule::filePath
    std::string_view filePathView = readStringView(ptr, bytesRead);
    result.filePath = filePathView;

    if (!result.isHeaderUnit)
    {
        return result;
    }

    // BTCNonModule::fileSize
    result.fileSize = readUint32(ptr, bytesRead);

    // BTCNonModule::logicalNames
    uint32_t logicalNamesCount = readUint32(ptr, bytesRead);
    result.logicalNames.reserve(logicalNamesCount);
    for (uint32_t i = 0; i < logicalNamesCount; ++i)
    {
        std::string_view sv = readStringView(ptr, bytesRead);
        result.logicalNames.emplace_back(sv);
    }

    // BTCNonModule::huDeps
    uint32_t huDepsCount = readUint32(ptr, bytesRead);
    result.huDeps.reserve(huDepsCount);
    for (uint32_t i = 0; i < huDepsCount; ++i)
    {
        P2978::HuDep dep;

        // HuDep::file::filePath
        dep.file.filePath = readStringView(ptr, bytesRead);

        // HuDep::file::fileSize
        dep.file.fileSize = readUint32(ptr, bytesRead);

        // HuDep::logicalNames
        uint32_t depLogicalNamesCount = readUint32(ptr, bytesRead);
        dep.logicalNames.reserve(depLogicalNamesCount);
        for (uint32_t j = 0; j < depLogicalNamesCount; ++j)
        {
            std::string_view sv = readStringView(ptr, bytesRead);
            dep.logicalNames.emplace_back(sv);
        }

        // HuDep::isSystem
        dep.isSystem = readBool(ptr, bytesRead);

        result.huDeps.emplace_back(std::move(dep));
    }

    // Sanity check
    if (bytesRead + strlen(P2978::delimiter) != buffer.size())
    {
        HMAKE_HMAKE_INTERNAL_ERROR
        /*std::cerr << "WARNING: Deserialized " << bytesRead << " bytes but buffer size is " << buffer.size()
                  << " (difference: " << (int)buffer.size() - (int)bytesRead << ")\n";*/
    }

    return result;
}

void CppMod::makeAndSendBTCNonModule(CppMod &hu)
{
    realBTargets[0].launchTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();

    hu.makeMemoryFileMapping();
    hu.populateAllDeps();

    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];
    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string toBeSend(&alloc);

    // BTCNonModule::isHeaderUnit
    writeBool(toBeSend, true);
    // BTCNonModule::isSystem
    writeBool(toBeSend, hu.target->isSystem);

    if (!firstMessageSent)
    {
        firstMessageSent = true;
        // BTCNonModule::headerFiles
        writeUint32(toBeSend, composingHeaders.size());
        for (auto &[str, composingNode] : composingHeaders)
        {
            // emplace in header-files to send
            // HeaderFile::logicalName
            writeStringView(toBeSend, str);
            // HeaderFile::filePath
            writeStringView(toBeSend, composingNode->filePath);
            toBeSend.push_back('\0');
            // HeaderFile::isSystem
            writeBool(toBeSend, target->isSystem);
        }
    }
    else
    {
        // BTCNonModule::headerFiles
        writeUint32(toBeSend, 0);
    }

    // BTCNonModule::filePath
    writeStringView(toBeSend, hu.interfaceNode->filePath);
    toBeSend.push_back('\0');
    // BTCNonModule::fileSize
    writeUint32(toBeSend, hu.interfaceFileSize);
    // BTCNonModule::logicalNames
    writeUint32(toBeSend, hu.logicalNames.size());
    for (const string &str : hu.logicalNames)
    {
        writeStringView(toBeSend, str);
    }

    // index of the place-holder size of huDeps
    uint32_t placeHolderIndex = toBeSend.size();

    // BTCNonModule::huDeps
    writeUint32(toBeSend, 0);

    uint32_t count = 0;
    for (const CppModWithDirect &transitive : hu.allCppModDeps)
    {
        CppMod *modDep = transitive.getPointer();
        if (!allCppModDeps.emplace(modDep, false).second)
        {
            continue;
        }

        ++count;
        modDep->makeMemoryFileMapping();

        // HuDep::file::filePath
        writeStringView(toBeSend, modDep->interfaceNode->filePath);
        toBeSend.push_back('\0');
        // HuDep::file::fileSize
        writeUint32(toBeSend, modDep->interfaceFileSize);
        // BTCNonModule::isSystem
        writeBool(toBeSend, modDep->target->isSystem);

        // HuDep::logicalNames
        writeUint32(toBeSend, modDep->logicalNames.size());
        for (const string &str : modDep->logicalNames)
        {
            writeStringView(toBeSend, str);
        }
    }
    *reinterpret_cast<uint32_t *>(&toBeSend[placeHolderIndex]) = count;
    toBeSend.append(P2978::delimiter, strlen(P2978::delimiter));

    if (const auto &r2 = ipcManager->writeInternal(toBeSend); !r2)
    {
        printErrorMessage(FORMAT("send-message fail of header-unit {}\n for {} {}\n of target {}\n.", hu.node->filePath,
                                 type == CppModType::HEADER_UNIT ? "header-unit" : "module-filee", node->filePath,
                                 target->name));
    }
}

CppMod *CppMod::findModule(const string_view moduleName) const
{
    if (const auto it = target->imodNames.find(moduleName); it != target->imodNames.end())
    {
        return it->second;
    }

    if (!moduleName.contains(':'))
    {
        for (CppTarget *req : target->reqDeps)
        {
            if (auto it2 = req->imodNames.find(moduleName); it2 != req->imodNames.end())
            {
                return it2->second;
            }
        }
    }

    return nullptr;
}

HeaderFileOrUnit CppMod::findHeaderFileOrUnit(const string_view headerName) const
{
    if (const auto &it = target->reqHeaderNameMapping.find(headerName); it != target->reqHeaderNameMapping.end())
    {
        return it->second;
    }

    if (const auto &it = target->configuration->headerNameMapping.find(headerName);
        it != target->configuration->headerNameMapping.end())
    {
        if (target->configuration->evaluate(UseConfigurationScope::YES))
        {
            // If configuration scope is set, then there can't be more than one entry in
            // Configuration::headerNameMapping. This is ensured at config-time.
            return it->second[0];
        }

        for (const vector<HeaderFileOrUnit> &configHeaderFilesOrUnits = it->second;
             const HeaderFileOrUnit &headerFileOrUnit : configHeaderFilesOrUnits)
        {
            if (std::ranges::find(target->cachedReqDeps, headerFileOrUnit.targetIndex) != target->cachedReqDeps.end())
            {
                // this headerFileOrUnit is provided by one of our dependency cpp-target.
                return headerFileOrUnit;
            }
        }
    }

    fflush(stdout);
    return {static_cast<Node *>(nullptr), false};
}

bool CppMod::isEventRegistered(Builder &builder)
{
    // an optimization is to increase/decrease the activeEventCount for less stack.
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        assert("CppMod::updateBTarget cannot be called in Configure Mode\n");
    }

    if (!selectiveBuild)
    {
        return false;
    }

    RealBTarget &rb = realBTargets[0];
    if (waitingFor)
    {
        return isEventCompleted(builder, string_view{});
    }

    isScheduled = true;

    if (rb.exitStatus != EXIT_SUCCESS)
    {
        return false;
    }

    if (huOnly && type != CppModType::HEADER_UNIT)
    {
        rb.exitStatus = EXIT_FAILURE;
        return false;
    }

    if (rb.updateStatus == UpdateStatus::UNCHECKED)
    {
        setFileStatus();
    }
    if (rb.updateStatus != UpdateStatus::UPDATED_NEEDED)
    {
        return false;
    }

    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];
    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string cppFullCompileCommand(&alloc);
    getCompileCommand(cppFullCompileCommand, target->useIPC ? CommandType::USE_IPC : CommandType::CONVENTIONAL, "");
    if (dryRun)
    {
        cppFullCompileCommand += '\n';
        printMessage(cppFullCompileCommand);
        return false;
    }

    if (!target->useIPC)
    {
        rb.launchTime =
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        run.startAsyncProcess(cppFullCompileCommand.c_str(), builder, this, false);
        originalLaunchTime = rb.launchTime;
        return true;
    }

    isAllDepsPopulated = true;

    rb.launchTime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    run.startAsyncProcess(cppFullCompileCommand.c_str(), builder, this, true);
    originalLaunchTime = rb.launchTime;
    ipcManager = new IPCManagerBS(run.writePipe);

    return true;
}

void CppMod::completeModuleCompilation(const Builder &builder)
{
    RealBTarget &rb = realBTargets[0];
    if (rb.exitStatus != EXIT_SUCCESS)
    {
        print(builder, *run.output);
        return;
    }

    if (target->useIPC)
    {
        // maybe move to where these are parsed
        for (auto &[str, headerFile] : composingHeaders)
        {
            headerFile->checkHashing = true;
        }
    }
    else
    {
    }

    if (type == CppModType::HEADER_UNIT || type == CppModType::PRIMARY_EXPORT || type == CppModType::PARTITION_EXPORT)
    {
        P2978::BMIFile file{.filePath = interfaceNode->filePath};
        if (const auto &r2 = IPCManagerBS::createSharedMemoryBMIFile(file); !r2)
        {
            printErrorMessage(FORMAT("Failed to create shared-memory BMI-file {}\nof target {}\n",
                                     interfaceNode->filePath, target->name));
        }
        interfaceFileSize = file.fileSize;
        memoryMappingCompleted = true;
    }

    if (target->useIPC)
    {
        if (target->configuration->evaluate(DuplicationWarning::YES))
        {
            for (auto &[str, headerFile] : composingHeaders)
            {
                if (const auto &[it, ok] = headerNodeCppMod.emplace(headerFile, this); !ok)
                {
                    printMessage(FORMAT("Warning! In target {}\nIn CppMod {}\n, header-file {}\n is also being "
                                        "provided by CppMod.\n{}\n",
                                        target->name, node->filePath, headerFile->filePath,
                                        it->second->node->filePath));
                }
            }

            for (const CppModWithDirect cppModWithDirect : allCppModDeps)
            {
                for (const CppMod *dep = cppModWithDirect.getPointer();
                     const auto &[headerFile, cppMod] : dep->headerNodeCppMod)
                {
                    if (const auto &[it, ok] = headerNodeCppMod.emplace(headerFile, cppMod); !ok)
                    {
                        printMessage(
                            FORMAT("Warning! In target {}\nIn CppMod {}\n, header-file {}\n is being included by "
                                   "2 CppMod.\n{}\n{}\n",
                                   target->name, node->filePath, headerFile->filePath, cppMod->node->filePath,
                                   it->second->node->filePath));
                    }
                }
            }
        }
    }

    buildCacheUpdated = true;
    buildFooterUpdated = true;
    print(builder, *run.output);
}

bool CppMod::isEventCompleted(Builder &builder, string_view message)
{
    // todo
    //  this is performance critical code. following improvements can be done.
    // 1) CppSrc::headerFiles can be made a vector or probable a pointer in a large list-buffer. this buffer can have
    // multiple lists of these header-files, and every CppSrc and CppMod having a pointer to the list. something like
    // Builder::updateBTargets.
    // 2) above string_view message can passed directly as part of output instead of first separating it out.
    // 3) Probably don't do colored output and copy from the Ninja. also take look for optimizing the header-file
    // parsing from msvc output and gcc .d files
    // 4) run.output can be reused to keep memory usage limited.

    RealBTarget &rb = realBTargets[0];
    if (!target->useIPC)
    {
        // todo
        // command currently does not add .d file and that .d file must be passed as-well.
        // parseHeaderDeps(*run.output, builder);
        completeModuleCompilation(builder);
        return false;
    }

    if (waitingFor)
    {
        ++builder.activeEventCount;
        if (waitingFor->realBTargets[0].exitStatus != EXIT_SUCCESS)
        {
            waitingFor = nullptr;
            run.killModuleProcess(builder);
            rb.exitStatus = EXIT_FAILURE;
            return false;
        }

        if (waitingFor->type == CppModType::HEADER_UNIT)
        {
            makeAndSendBTCNonModule(*waitingFor);
        }
        else
        {
            makeAndSendBTCModule(*waitingFor);
        }

        waitingFor = nullptr;

        // Wait for more input
        return true;
    }

    if (message.empty())
    {
        completeModuleCompilation(builder);
        return false;
    }

    char buffer[320];
    P2978::CTB requestType;
    if (const auto &r = IPCManagerBS::receiveMessage(buffer, requestType, message); !r)
    {
        printErrorMessage(
            FORMAT("receive-message fail for module-file {}\n of target {}\n.", node->filePath, target->name));
    }

    CppMod *found;

    if (requestType == P2978::CTB::NON_MODULE)
    {
        auto &[isHURequested, headerName] = reinterpret_cast<P2978::CTBNonModule &>(buffer);

        HeaderFileOrUnit f = findHeaderFileOrUnit(headerName);
        if (!f.data.cppMod)
        {
            printErrorMessage(FORMAT("No File in the target\n{}\n or in its dependencies\n{}\n provides this "
                                     "header \n{}\n requested in {}\n",
                                     target->name, target->getDependenciesString(), headerName, node->filePath));
        }

        // Checking if this is a big header-unit with composing header-files. Composing headers should be
        // included in the big header with same logical-name as they are meant to be used in other files. So we
        // can use the same headerName to search whether we have a composing header specified. Otherwise, it
        // would be diagnosed as cyclic dependency.
        if (f.data.cppMod == this && !firstMessageSent)
        {
            if (const auto it = composingHeaders.find(headerName); it != composingHeaders.end())
            {
                f = HeaderFileOrUnit{(it->second), false};
            }
        }

        if (f.isUnit)
        {
            found = f.data.cppMod;
        }
        else
        {
            if (isHURequested)
            {
                printErrorMessage(FORMAT("Could not find the header-unit {} requested by file {}\n in target {}.\n",
                                         headerName, node->filePath, target->name));
            }

            constexpr uint32_t stackSize = 64 * 1024;
            char buffer[stackSize];
            std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
            std::pmr::string toBeSend(&alloc);

            // BTCNonModule::isHeaderUnit
            writeBool(toBeSend, false);
            // BTCNonModule::isSystem
            writeBool(toBeSend, f.isSystem);
            uint32_t placeHolderIndex = toBeSend.size();
            uint32_t count = 0;

            bool addedInComposingHeader = false;
            if (!firstMessageSent)
            {
                writeUint32(toBeSend, -1); // placeholder
                firstMessageSent = true;
                for (const auto &[str, composingNode] : composingHeaders)
                {
                    if (f.data.node == composingNode && headerName == str)
                    {
                        addedInComposingHeader = true;
                        continue;
                    }

                    ++count;

                    writeStringView(toBeSend, str);
                    // HeaderFile::filePath
                    writeStringView(toBeSend, composingNode->filePath);
                    toBeSend.push_back('\n');
                    // HeaderFile::isSystem
                    writeBool(toBeSend, target->isSystem);
                }
                *reinterpret_cast<uint32_t *>(&toBeSend[placeHolderIndex]) = count;
            }
            else
            {
                writeUint32(toBeSend, 0);
            }

            // BTCNonModule::filePath
            writeStringView(toBeSend, f.data.node->filePath);
            toBeSend.push_back('\n');
            toBeSend.append(P2978::delimiter, strlen(P2978::delimiter));

            if (const auto &r2 = ipcManager->writeInternal(toBeSend); !r2)
            {
                printErrorMessage(FORMAT("send-message fail of header-file {}\n for module-file {}\n of target {}\n.",
                                         f.data.node->filePath, node->filePath, target->name));
            }

            if (!addedInComposingHeader)
            {
                if (!composingHeaders.emplace(headerName, f.data.node).second)
                {
                    printErrorMessage(FORMAT("An already sent header-file \n{}\n re-requested in file.\n{}\n",
                                             f.data.node->filePath, node->filePath));
                }
            }

            return true;
        }
    }
    else
    {
        string_view moduleName = reinterpret_cast<P2978::CTBModule &>(buffer).moduleName;
        found = findModule(moduleName);

        if (!found)
        {
            if (moduleName.contains(':'))
            {
                printErrorMessage(FORMAT("No File in the target\n{}\n provides this module\n{}.\n requested in file {}",
                                         target->name, moduleName, node->filePath));
            }
            else
            {
                printErrorMessage(FORMAT("No File in the target\n{}\n or in its dependencies\n{}\n provides "
                                         "this module\n{}.\n requested in file {}",
                                         target->name, target->getDependenciesString(), moduleName, node->filePath));
            }
        }
    }

    const auto &[it, ok] = allCppModDeps.emplace(found, true);

    if (!ok)
    {
        printErrorMessage(FORMAT("Warning: already sent the module {}\n with logical-name{}\n requested in {}\n.",
                                 found->node->filePath, found->logicalNames[0], node->filePath));
    }

    RealBTarget &foundRb = found->realBTargets[0];

    if (!foundRb.isCompleted)
    {
        waitingFor = found;

        // This is the only place where dependency-relationship is being added dynamically. We are sure that
        // decrementFromDependents has not been called for our dependency.
        foundRb.dependents.emplace(&rb, RelationType::FULL, BTargetType::CPP_MOD);
        ++rb.dependenciesSize;

        // if its dependenciesSize is zero, it means that it is already in the list. We just bring it to the front.
        if (!found->isScheduled && foundRb.dependenciesSize == 0)
        {
            found->isScheduled = true;
            // Old index is reset and then we re-add
            builder.updateBTargets.array[foundRb.insertionIndex].value = nullptr;
            uint32_t insertionIndex = 0;
            builder.updateBTargets.emplace(&foundRb, insertionIndex);
            foundRb.insertionIndex = insertionIndex; // not needed probably
        }
        ++builder.updateBTargetsSizeGoal;
        // This process is going to idle. Build-system will automatically decrement when it launches a new process.
        ++builder.idleCount;
        --builder.activeEventCount;
        return true;
    }

    if (foundRb.exitStatus != EXIT_SUCCESS)
    {
        run.killModuleProcess(builder);
        rb.exitStatus = EXIT_FAILURE;
        return false;
    }

    if (requestType == P2978::CTB::MODULE)
    {
        makeAndSendBTCModule(*found);
    }
    else
    {
        makeAndSendBTCNonModule(*found);
    }

    // Requested module or header-unit has been sent. Let's get the next message.
    return true;
}

void CppMod::print(const Builder &builder, const string &output) const
{
    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];
    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string outputStr(&alloc);
    if (isConsole)
    {
        outputStr += getColorCode(type == CppModType::HEADER_UNIT ? ColorIndex::hot_pink : ColorIndex::magenta);
    }

    if (output.empty())
    {
        outputStr += FORMAT("[{}/{}]C++{} {} {}", builder.updatedCount, builder.updateBTargetsSizeGoal,
                            type == CppModType::HEADER_UNIT ? "Header-Unit" : "Module", node->filePath, target->name);
    }
    else
    {
        getCompileCommand(outputStr, target->useIPC ? CommandType::USE_IPC : CommandType::CONVENTIONAL, "");
    }

    outputStr += ' ';

    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::reset);
    }

    outputStr += output;
    outputStr.push_back('\n');

    fwrite(outputStr.c_str(), 1, outputStr.size(), stdout);
}

void CppMod::getCompileCommand(std::pmr::string &compileCommand, const CommandType commandType,
                               const string_view mockFilePath) const
{
    if (sourceType != SourceType::CPP && type != CppModType::PRIMARY_IMPLEMENTATION)
    {
        printErrorMessage(FORMAT("Module-File {}\nof CppTarget {}\n is implementation-unit but not a C++ file\n",
                                 node->filePath, target->name));
    }

    if (sourceType == SourceType::CPP)
    {
        compileCommand = target->configuration->cppCompileCommand;
    }
    else if (sourceType == SourceType::C)
    {
        compileCommand = target->configuration->cCompileCommand;
    }
    else if (sourceType == SourceType::ASSEMBLY)
    {
        compileCommand = target->configuration->assemblyCompileCommand;
    }

    target->setCompileCommand(compileCommand);
    compileCommand += "-Wno-experimental-header-units ";

    // if addMockFile is true, then -fuseIPC="mock-file-path" is used instead of -useIPC
    string useIPCsTR;
    if (commandType == CommandType::USE_IPC_MOCK_FILE)
    {
        useIPCsTR = "-useIPC=\"";
        useIPCsTR += mockFilePath;
        useIPCsTR += "\" ";
    }
    else if (commandType == CommandType::USE_IPC)
    {
        useIPCsTR = "-useIPC ";
    }
    if (const Compiler &c = target->configuration->compilerFeatures.compiler;
        c.bTFamily == BTFamily::MSVC && c.btSubFamily == BTSubFamily::CLANG)
    {
        if (type == CppModType::HEADER_UNIT)
        {
            compileCommand +=
                (target->isSystem ? "-fmodule-header=system /clang:-o\"" : "-fmodule-header=user /clang:-o\"") +
                interfaceNode->filePath + "\" " + useIPCsTR + "-x c++-header \"" + node->filePath + '\"';
        }
        else if (type == CppModType::PRIMARY_EXPORT || type == CppModType::PARTITION_EXPORT)
        {
            compileCommand += " -o \"" + objectNode->filePath + "\" " + useIPCsTR + "-c -x c++-module \"" +
                              node->filePath + "\" -fmodule-output=\"" + interfaceNode->filePath + '\"';
        }
        else
        {
            compileCommand += "-o \"" + objectNode->filePath + "\" " + useIPCsTR + "-c /TP \"" + node->filePath + '\"';
        }

        if (isConsole)
        {
            compileCommand += " -fdiagnostics-color=always";
        }
        else
        {
            compileCommand += " -fdiagnostics-color=never";
        }
    }
    else if (c.bTFamily == BTFamily::GCC && c.btSubFamily == BTSubFamily::CLANG)
    {
        compileCommand += commandType == CommandType::CONVENTIONAL ? "" : "-nostdinc -nostdinc++ ";
        if (type == CppModType::HEADER_UNIT)
        {
            compileCommand += (target->isSystem ? "-fmodule-header=system -o\"" : "-fmodule-header=user -o\"") +
                              interfaceNode->filePath + "\" " + useIPCsTR + "-x c++-header \"" + node->filePath + '\"';
        }
        else if (type == CppModType::PRIMARY_EXPORT || type == CppModType::PARTITION_EXPORT)
        {
            compileCommand += " -o \"" + objectNode->filePath + "\" " + useIPCsTR + "-c -x c++-module \"" +
                              node->filePath + "\" -fmodule-output=\"" + interfaceNode->filePath + '\"';
        }
        else
        {
            compileCommand += "-o \"" + objectNode->filePath + "\" " + useIPCsTR + "-c \"" + node->filePath + '\"';
        }

        if (isConsole)
        {
            compileCommand += " -fdiagnostics-color=always ";
        }
        else
        {
            compileCommand += " -fdiagnostics-color=never ";
        }
    }

    if (commandType != CommandType::CONVENTIONAL)
    {
        return;
    }

    // Only for convention command-line approach if the compiler supports such.
    FOR_DEPS(*this, 0, BTargetType::CPP_MOD, CppMod, mod)
    {
        if (mod->type == CppModType::PRIMARY_IMPLEMENTATION)
        {
            continue;
        }
        compileCommand += "-fmodule-file=\"" + mod->interfaceNode->filePath + "\" ";
    }
}

void CppMod::setFileStatus()
{
    RealBTarget &rb = realBTargets[0];
    assert(rb.updateStatus == UpdateStatus::UNCHECKED);

    if (node->fileType == file_type::not_found)
    {
        string str;
        if (type == CppModType::HEADER_UNIT)
        {
            str = "C++HeaderUnit";
        }
        else if (type == CppModType::PRIMARY_IMPLEMENTATION)
        {
            str = "C++Module";
        }
        else
        {
            str = "C++InterfaceModule";
        }

        printErrorMessage(FORMAT("{} {}\n of target {}\n not found.\n", str, node->filePath, target->name));
    }

    rb.updateStatus = UpdateStatus::UPDATED_NEEDED;

    if (type == CppModType::HEADER_UNIT)
    {
        if (interfaceNode->fileType == file_type::not_found)
        {
            return;
        }
    }
    else if (type == CppModType::PRIMARY_IMPLEMENTATION)
    {
        if (objectNode->fileType == file_type::not_found)
        {
            return;
        }
    }
    else
    {
        if (interfaceNode->fileType == file_type::not_found || objectNode->fileType == file_type::not_found)
        {
            return;
        }
    }

    for (const uint32_t depIndex : cachedDeps)
    {
        CppMod *cppMod = static_cast<CppMod *>(fileTargetCaches[depIndex].bTarget);

        // Can happen because the export-name or the include-name got mapped to a different file in the same target.
        if (!cppMod)
        {
            return;
        }

        const RealBTarget *depRb = &cppMod->realBTargets[0];
        if (depRb->updateStatus == UpdateStatus::UNCHECKED)
        {
            cppMod->setFileStatus();
        }

        if (depRb->updateStatus == UpdateStatus::UPDATED_NEEDED)
        {
            return;
        }

        if (depRb->launchTime > rb.launchTime)
        {
            return;
        }
    }

    rb.updateStatus = UpdateStatus::UNCHECKED;

    // pmr
    std::vector<uint64_t> contentHashes;
    contentHashes.reserve(1 + 1 + cachedHeaderFiles.size());
    contentHashes.emplace_back(commandHash);
    contentHashes.emplace_back(node->contentHash);
    for (const uint32_t nodeIndex : cachedHeaderFiles)
    {
        contentHashes.emplace_back(Node::getHalfNode(nodeIndex)->contentHash);
    }
    rb.cumulativeHash = rapidhash(contentHashes.data(), contentHashes.size() * 8);

    ObjectFile::setFileStatus();
}

void CppMod::generateStandAloneCommand()
{
    if (target->configuration->evaluate(StandAloneCommand::YES))
    {
        /*if (const RealBTarget &rb = realBTargets[0];
            rb.updateStatus == UpdateStatus::UPDATED ||
            (rb.updateStatus == UpdateStatus::UPDATED_WITHOUT_BUILDING && rb.exitStatus != EXIT_SUCCESS))
        {
            path scriptDirectory = target->myBuildDir->filePath;
            scriptDirectory /= node->getFileName() + toString(node->myId);
            std::filesystem::create_directory(scriptDirectory);
            string scriptContents =
                FORMAT("#!/bin/bash\n\nset -x\n\n# This script compiles {}. Run it in the build-dir with same "
                       "name as current build-dir.\n\n",
                       node->filePath);
            flat_hash_set<string> createdDirs;
            cppStandAloneCommand(createdDirs, scriptContents, scriptDirectory.string());
            std::ofstream(scriptDirectory / "script.sh") << scriptContents;
        }*/
    }
}

void CppMod::cppStandAloneCommand(flat_hash_set<string> &createdDirs, string &scriptContents, const string &scriptDir)
{
    btree_set<BTarget *, IndexInTopologicalSortComparatorRoundTwo> allDeps;

    for (const RBTWithType &rbt : realBTargets[0].dependencies)
    {
        allDeps.emplace(rbt.getPointer()->getBTarget());
    }

    FOR_DEPS(*this, 0, BTargetType::CPP_MOD, CppMod, cppMod)
    {
        allDeps.emplace(cppMod);
        for (const RBTWithType &rbt : cppMod->realBTargets[0].dependencies)
        {
            allDeps.emplace(rbt.getPointer()->getBTarget());
        }
    }

    // This step is done so that if there are custom code generation steps, then those could be added to the script file
    // as well.
    for (auto it = allDeps.rbegin(); it != allDeps.rend(); ++it)
    {
        (*it)->cppStandAloneCommand(createdDirs, scriptContents, scriptDir);
    }

    if (createdDirs.emplace(target->configuration->name).second)
    {
        scriptContents += "mkdir " + target->configuration->name + '\n';
    }

    if (createdDirs.emplace(target->name).second)
    {
        scriptContents += "mkdir " + target->name + '\n';
    }

    if (!target->useIPC)
    {
        constexpr uint32_t stackSize = 64 * 1024;
        char buffer[stackSize];
        std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
        std::pmr::string cppFullCompileCommand(&alloc);

        getCompileCommand(cppFullCompileCommand,
                          target->useIPC ? CommandType::USE_IPC_MOCK_FILE : CommandType::CONVENTIONAL, "");

        scriptContents += cppFullCompileCommand;
        scriptContents.push_back('\n');
        return;
    }

    const string mockFilePath = (path(scriptDir) / FORMAT("mock-file{}.bin", id)).string();
    {
        // New scope for mock-file.bin
        constexpr uint32_t stackSize = 256 * 1024;
        char buffer[stackSize];
        std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
        std::pmr::string mockFileContents(&alloc);

        flat_hash_set<string> ignoreNames;

        uint32_t count = 0;
        writeUint32(mockFileContents, count);

        FOR_DEPS(*this, 0, BTargetType::CPP_MOD, CppMod, cppMod)
        {
            if (cppMod->type == CppModType::PRIMARY_IMPLEMENTATION)
            {
                continue;
            }
            for (const string &logicalName : cppMod->logicalNames)
            {
                ignoreNames.emplace(logicalName);
                ++count;
                writeStringView(mockFileContents, logicalName);
                writeStringView(mockFileContents, cppMod->interfaceNode->filePath);
                mockFileContents.push_back('\0');
                P2978::FileType file;
                if (cppMod->type == CppModType::HEADER_UNIT)
                {
                    file = P2978::FileType::HEADER_UNIT;
                }
                else
                {
                    file = P2978::FileType::MODULE;
                }
                writeUint8(mockFileContents, static_cast<uint8_t>(file));
                writeBool(mockFileContents, cppMod->target->isSystem);
            }
        }

        for (auto &[str, headerFile] : composingHeaders)
        {
            if (ignoreNames.emplace(str).second)
            {
                ++count;
                writeStringView(mockFileContents, str);
                writeStringView(mockFileContents, headerFile->filePath);
                mockFileContents.push_back('\0');
                writeUint8(mockFileContents, static_cast<uint8_t>(FileType::HEADER_FILE));
                writeBool(mockFileContents, target->isSystem);
            }
        }

        *reinterpret_cast<uint32_t *>(mockFileContents.data()) = count;
        std::ofstream(mockFilePath) << mockFileContents;
    }

    constexpr uint32_t stackSize = 64 * 1024;
    char buffer[stackSize];
    std::pmr::monotonic_buffer_resource alloc(buffer, stackSize);
    std::pmr::string cppFullCompileCommand(&alloc);

    getCompileCommand(cppFullCompileCommand,
                      target->useIPC ? CommandType::USE_IPC_MOCK_FILE : CommandType::CONVENTIONAL, mockFilePath);

    scriptContents += cppFullCompileCommand;
    scriptContents.push_back('\n');
}

void CppMod::writeConfigCacheAtConfigTime(string &buffer)
{
    const string fileNumber = toString(node->myId);
    const bool isHU = type == CppModType::HEADER_UNIT;
    const bool isImpl = type == CppModType::PRIMARY_IMPLEMENTATION;

    if (!isImpl)
    {
        interfaceNode = Node::getNode(target->myBuildDir->filePath + slashc + node->getFileName() + fileNumber + ".ifc",
                                      true, true);
        writeNode(buffer, interfaceNode);
        if (!isHU)
        {
            writeStringView(buffer, logicalNames[0]);
        }
    }

    if (!isHU)
    {
        objectNode =
            Node::getNode(target->myBuildDir->filePath + slashc + node->getFileName() + fileNumber + ".o", true, true);
        writeNode(buffer, objectNode);
        writeUint32(buffer, logicalNames.size());
        for (const string &str : logicalNames)
        {
            writeStringView(buffer, str);
        }
    }
    else
    {
        writeBool(buffer, isReqHu);
        writeBool(buffer, isUseReqHu);
        writeUint32(buffer, composingHeaders.size());
        writeUint32(buffer, logicalNames.size());
        if (target->useIPC)
        {
            for (const auto &[headerName, headerNode] : composingHeaders)
            {
                writeStringView(buffer, headerName);
                writeNode(buffer, headerNode);
            }
        }
        else
        {
            for (const auto &[headerName, headerNode] : composingHeaders)
            {
                writeStringView(buffer, headerName);
            }
        }
        for (const string &str : logicalNames)
        {
            writeStringView(buffer, str);
        }
    }
}

void CppMod::verifyConfigCache(const string_view configCache) const
{
    uint32_t bytesRead = 0;

    const bool isHU = type == CppModType::HEADER_UNIT;
    const bool isImpl = type == CppModType::PRIMARY_IMPLEMENTATION;

    if (!isImpl)
    {
        if (const Node *cachedInterfaceNode = readHalfNode(configCache.data(), bytesRead);
            interfaceNode != cachedInterfaceNode)
        {
            printErrorMessage(FORMAT("{} configCache-verification failed: interfaceNode mismatch\n"
                                     "current=\"{}\"  cached=\"{}\"\n",
                                     getPrintName(), interfaceNode ? interfaceNode->filePath : "<null>",
                                     cachedInterfaceNode ? cachedInterfaceNode->filePath : "<null>"));
        }

        if (!isHU)
        {
            const string_view cachedLogicalName = readStringView(configCache.data(), bytesRead);
            if (logicalNames.empty() || logicalNames[0] != cachedLogicalName)
            {
                printErrorMessage(FORMAT("{} configCache-verification failed: logicalName mismatch\n"
                                         "current=\"{}\"  cached=\"{}\"\n",
                                         getPrintName(), logicalNames.empty() ? "<empty>" : logicalNames[0],
                                         cachedLogicalName));
            }
        }
    }

    if (!isHU)
    {
        const Node *cachedObjectNode = readHalfNode(configCache.data(), bytesRead);
        if (objectNode != cachedObjectNode)
        {
            printErrorMessage(FORMAT("{} configCache-verification failed: objectNode mismatch\n"
                                     "current=\"{}\"  cached=\"{}\"\n",
                                     getPrintName(), objectNode ? objectNode->filePath : "<null>",
                                     cachedObjectNode ? cachedObjectNode->filePath : "<null>"));
        }
    }
    else
    {
        const bool cachedIsReqHu = readBool(configCache.data(), bytesRead);
        if (isReqHu != cachedIsReqHu)
        {
            printErrorMessage(FORMAT("{} configCache-verification failed: isReqHu mismatch current={} cached={}\n",
                                     getPrintName(), isReqHu, cachedIsReqHu));

            const uint32_t cachedLogicalNamesSize = readUint32(configCache.data(), bytesRead);
            if (logicalNames.size() != cachedLogicalNamesSize)
            {
                printErrorMessage(
                    FORMAT("{} configCache-verification failed: logicalNames size mismatch current={} cached={}\n",
                           getPrintName(), logicalNames.size(), cachedLogicalNamesSize));
            }

            for (uint32_t i = 0; i < cachedLogicalNamesSize; ++i)
            {
                const string_view cachedName = readStringView(configCache.data(), bytesRead);
                const string_view currentName = (i < logicalNames.size()) ? string_view(logicalNames[i]) : "<missing>";
                if (currentName != cachedName)
                {
                    printErrorMessage(FORMAT("{} configCache-verification failed: logicalName[{}] mismatch\n"
                                             "current=\"{}\"  cached=\"{}\"\n",
                                             getPrintName(), i, currentName, cachedName));
                }
            }
        }

        const bool cachedIsUseReqHu = readBool(configCache.data(), bytesRead);
        if (isUseReqHu != cachedIsUseReqHu)
        {
            printErrorMessage(FORMAT("{} configCache-verification failed: isUseReqHu mismatch current={} cached={}\n",
                                     getPrintName(), isUseReqHu, cachedIsUseReqHu));
        }

        const uint32_t cachedComposingHeadersSize = readUint32(configCache.data(), bytesRead);
        if (composingHeaders.size() != cachedComposingHeadersSize)
        {
            printErrorMessage(
                FORMAT("{} configCache-verification failed: composingHeaders size mismatch current={} cached={}\n",
                       getPrintName(), composingHeaders.size(), cachedComposingHeadersSize));
        }

        const uint32_t cachedLogicalNamesSize = readUint32(configCache.data(), bytesRead);
        if (logicalNames.size() != cachedLogicalNamesSize)
        {
            printErrorMessage(
                FORMAT("{} configCache-verification failed: logicalNames size mismatch current={} cached={}\n",
                       getPrintName(), logicalNames.size(), cachedLogicalNamesSize));
        }

        for (uint32_t i = 0; i < cachedComposingHeadersSize; ++i)
        {
            const string_view cachedHeaderName = readStringView(configCache.data(), bytesRead);
            const auto it = composingHeaders.find(cachedHeaderName);
            if (it == composingHeaders.end())
            {
                printErrorMessage(FORMAT("{} configCache-verification failed: cached composingHeader[{}] name\n\"{}\"\n"
                                         "is not present in current composingHeaders\n",
                                         getPrintName(), i, cachedHeaderName));
            }

            if (target->useIPC)
            {
                const Node *cachedHeaderNode = readHalfNode(configCache.data(), bytesRead);
                if (it != composingHeaders.end() && it->second != cachedHeaderNode)
                {
                    printErrorMessage(
                        FORMAT("{} configCache-verification failed: composingHeader \"{}\" node mismatch\n"
                               "current=\"{}\"  cached=\"{}\"\n",
                               getPrintName(), cachedHeaderName, it->second ? it->second->filePath : "<null>",
                               cachedHeaderNode ? cachedHeaderNode->filePath : "<null>"));
                }
            }
        }

        for (uint32_t i = 0; i < cachedLogicalNamesSize; ++i)
        {
            const string_view cachedName = readStringView(configCache.data(), bytesRead);
            const string_view currentName = (i < logicalNames.size()) ? string_view(logicalNames[i]) : "<missing>";
            if (currentName != cachedName)
            {
                printErrorMessage(FORMAT("{} configCache-verification failed: logicalName[{}] mismatch\n"
                                         "current=\"{}\"  cached=\"{}\"\n",
                                         getPrintName(), i, currentName, cachedName));
            }
        }
    }

    if (configCache.size() != bytesRead)
    {
        printErrorMessage(FORMAT("{} configCache-verification failed as configCache.size() != bytesRead {} vs {}\n",
                                 getPrintName(), configCache.size(), bytesRead));
    }
}

void CppMod::writeBuildCacheAtConfigTime(string &buffer)
{
    // headerStatusChanged
    writeBool(buffer, true);
    // sizeof header-files
    writeUint32(buffer, 0);
    // sizeof cppMod-deps
    writeUint32(buffer, 0);
}

void CppMod::writeBuildCacheAtBuildTime(string &buffer)
{
    RealBTarget &rb = realBTargets[0];

    // pmr
    std::vector<uint64_t> contentHashes;
    contentHashes.reserve(1 + 1 + (target->useIPC ? composingHeaders.size() : headerFiles.size()));
    contentHashes.emplace_back(commandHash);
    contentHashes.emplace_back(node->contentHash);

    if (target->useIPC)
    {
        for (const auto &[includeName, headerNode] : composingHeaders)
        {
            if (headerNode->lastWriteTime > originalLaunchTime)
            {
                // File was modified after process launched — hash is stale.
                contentHashes.emplace_back(0);
            }
            else
            {
                contentHashes.emplace_back(headerNode->contentHash);
            }
        }
    }
    else
    {
        for (Node *headerNode : headerFiles)
        {
            if (headerNode->lastWriteTime > originalLaunchTime)
            {
                // File was modified after process launched — hash is stale.
                contentHashes.emplace_back(0);
            }
            else
            {
                contentHashes.emplace_back(headerNode->contentHash);
            }
        }
    }
    rb.cumulativeHash = rapidhash(contentHashes.data(), contentHashes.size() * 8);

    // headerStatusChanged. directly written as false
    writeBool(buffer, false);
    if (target->useIPC)
    {
        writeUint32(buffer, composingHeaders.size());
        for (const auto &[includeName, headerNode] : composingHeaders)
        {
            writeNode(buffer, headerNode);
        }
    }
    else
    {
        // sizeof header-files
        writeUint32(buffer, headerFiles.size());
        for (const Node *header : headerFiles)
        {
            writeNode(buffer, header);
        }
    }

    const uint32_t currentSize = buffer.size();
    uint32_t count = 0;
    // placeholder for direct-deps count;
    writeUint32(buffer, 0);

    // make this pmr string
    string cacheBuffer;
    for (const CppModWithDirect &cppModDirect : allCppModDeps)
    {
        if (cppModDirect.isDirect())
        {
            ++count;
            writeUint32(buffer, cppModDirect.getPointer()->cacheIndex);
        }
    }

    memcpy(buffer.data() + currentSize, &count, sizeof(count));
}

void CppMod::verifyBuildCache(const string_view buildCache) const
{
    const RealBTarget &rb = realBTargets[0];

    if constexpr (bsMode == BSMode::BUILD)
    {
        // Recompute cumulativeHash and dump to debug file for comparison.
        std::vector<uint64_t> contentHashes;
        contentHashes.reserve(1 + 1 + (target->useIPC ? composingHeaders.size() : headerFiles.size()));
        contentHashes.emplace_back(commandHash);
        contentHashes.emplace_back(node->contentHash);

        if (target->useIPC)
        {
            for (const auto &[includeName, headerNode] : composingHeaders)
            {
                if (headerNode->lastWriteTime > originalLaunchTime)
                {
                    contentHashes.emplace_back(0);
                }
                else
                {
                    contentHashes.emplace_back(headerNode->contentHash);
                }
            }
        }
        else
        {
            for (const Node *headerNode : headerFiles)
            {
                if (headerNode->lastWriteTime > originalLaunchTime)
                {
                    contentHashes.emplace_back(0);
                }
                else
                {
                    contentHashes.emplace_back(headerNode->contentHash);
                }
            }
        }

        {
            const uint64_t recomputedHash = rapidhash(contentHashes.data(), contentHashes.size() * 8);
            const path debugFile =
                target->myBuildDir->filePath + slashc + string("hashes") + toString(node->myId) + ".txt";
            if (std::ofstream out(debugFile, std::ios::app); out)
            {
                out << "commandHash:       " << commandHash << '\n';
                out << "node->contentHash: " << node->contentHash << '\n';
                if (target->useIPC)
                {
                    uint32_t i = 0;
                    for (const auto &[includeName, headerNode] : composingHeaders)
                    {
                        out << "composingHeader[" << i << "] " << includeName
                            << " node=" << (headerNode ? headerNode->filePath : "<null>")
                            << " hash=" << contentHashes[i + 2] << '\n';
                        ++i;
                    }
                }
                else
                {
                    uint32_t i = 0;
                    for (const Node *headerNode : headerFiles)
                    {
                        out << "header[" << i << "] " << (headerNode ? headerNode->filePath : "<null>")
                            << " hash=" << contentHashes[i + 2] << '\n';
                        ++i;
                    }
                }
                out << "recomputedHash: " << recomputedHash << '\n';
                out << "storedHash:     " << rb.cumulativeHash << '\n';
            }

            if (recomputedHash != rb.cumulativeHash)
            {
                printErrorMessage(FORMAT("{} caching-verification failed as recomputed cumulativeHash != stored\n"
                                         "recomputed={} stored={}\n",
                                         getPrintName(), recomputedHash, rb.cumulativeHash));
            }
        }
    }
    uint32_t bytesRead = 0;

    const bool cachedHeaderStatusChanged = readBool(buildCache.data(), bytesRead);
    if (cachedHeaderStatusChanged)
    {
        if constexpr (bsMode == BSMode::BUILD)
        {
            printErrorMessage(FORMAT("{} caching-verification failed as headerStatusChanged is true at build time\n",
                                     getPrintName()));
        }
    }

    if (target->useIPC)
    {
        const uint32_t cachedComposingHeadersSize = readUint32(buildCache.data(), bytesRead);
        if (composingHeaders.size() != cachedComposingHeadersSize)
        {
            printErrorMessage(
                FORMAT("{} caching-verification failed as composingHeaders.size() != cached composingHeadersSize\n"
                       "current={} cached={}\n",
                       getPrintName(), composingHeaders.size(), cachedComposingHeadersSize));
        }

        for (uint32_t i = 0; i < cachedComposingHeadersSize; ++i)
        {
            const Node *cachedNode = readHalfNode(buildCache.data(), bytesRead);
            const auto it = std::find_if(composingHeaders.begin(), composingHeaders.end(),
                                         [cachedNode](const auto &kv) { return kv.second == cachedNode; });
            if (it == composingHeaders.end())
            {
                printErrorMessage(
                    FORMAT("{} caching-verification failed as cached composingHeader node\n{} at index {}\n"
                           "is not present in current composingHeaders\n",
                           getPrintName(), cachedNode ? cachedNode->filePath : "<null>", i));
            }
        }
    }
    else
    {
        const uint32_t cachedHeaderFilesSize = readUint32(buildCache.data(), bytesRead);
        if (headerFiles.size() != cachedHeaderFilesSize)
        {
            printErrorMessage(FORMAT("{} caching-verification failed as headerFiles.size() != cached headerFilesSize\n"
                                     "current={} cached={}\n",
                                     getPrintName(), headerFiles.size(), cachedHeaderFilesSize));
        }

        for (uint32_t i = 0; i < cachedHeaderFilesSize; ++i)
        {
            const Node *cachedNode = readHalfNode(buildCache.data(), bytesRead);
            if (!headerFiles.contains(cachedNode))
            {
                printErrorMessage(FORMAT("{} caching-verification failed as cached headerFile\n{} at index {}\n"
                                         "is not present in current headerFiles\n",
                                         getPrintName(), cachedNode ? cachedNode->filePath : "<null>", i));
            }
        }
    }
    return;

    uint32_t cachedDirectDepsCount = readUint32(buildCache.data(), bytesRead);
    uint32_t count = 0;
    for (const CppModWithDirect &cppModDirect : allCppModDeps)
    {
        if (cppModDirect.isDirect())
        {
            const uint32_t cachedCacheIndex = readUint32(buildCache.data(), bytesRead);
            if (cppModDirect.getPointer()->cacheIndex != cachedCacheIndex)
            {
                printErrorMessage(
                    FORMAT("{} caching-verification failed as direct dep cacheIndex does not match cached\n"
                           "current={} cached={} at index {}\n",
                           getPrintName(), cppModDirect.getPointer()->cacheIndex, cachedCacheIndex, count));
            }
            ++count;
        }
    }

    if (count != cachedDirectDepsCount)
    {
        printErrorMessage(FORMAT("{} caching-verification failed as direct deps count != cached\n"
                                 "current={} cached={}\n",
                                 getPrintName(), count, cachedDirectDepsCount));
    }

    verifyBTargetHeader(buildCache, bytesRead);

    if (buildCache.size() != bytesRead)
    {
        printErrorMessage(FORMAT("{} caching-verification failed as buildCache.size() != bytesRead {} vs {}\n",
                                 getPrintName(), buildCache.size(), bytesRead));
    }
}
