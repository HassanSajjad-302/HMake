
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
        const string_view configCache = bTargetCaches[cacheIndex].configCache;
        objectNode = readHalfNode(configCache.data(), bytesRead);

        if (4 != configCache.size())
        {
            HMAKE_HMAKE_INTERNAL_ERROR
        }
    }

    uint32_t bytesRead = 0;

    const string_view buildCache = bTargetCaches[cacheIndex].getBuildCache();
    const char *ptr = buildCache.data();

    const_cast<Node *>(node)->doHashFile = true;

    const uint32_t headerFilesSize = readUint32(ptr, bytesRead);

    cachedHeaderFiles = span{reinterpret_cast<const uint32_t *>(ptr + bytesRead), headerFilesSize};
    bytesRead += headerFilesSize * 4;
    for (const uint32_t headerNode : cachedHeaderFiles)
    {
        Node::getHalfNode(headerNode)->doHashFile = true;
    }

    if (bytesRead != buildCache.size())
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }

    objectNode->doStatFile = true;
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
            if (string_view line = string_view(str).substr(start, i - start + 1); !line.contains(includeFileNote))
            {
                output += string(line);
            }
        }
        return;
    }

    uint64_t startPos = 0;
    uint64_t lineEnd;
    string_view line;

    STACK_PMR_STRING(treatedOutput, 128 * 1024)

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
                printErrorMessage(FORMAT("Dependency output contains an empty header path.\nTarget: {}\n"
                                         "Source file: {}\nCompiler output line: {}",
                                         target->name, node->filePath, std::string(line)));
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

void CppSrc::parseHeadersFromGccDepsOutput(Builder &builder)
{
    string headerDepsFile = objectNode->filePath;
    // replacing .o ext with .d
    headerDepsFile[headerDepsFile.size() - 1] = 'd';

    STACK_PMR_STRING(headerFileDeps, 128 * 1024)
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
            parseHeadersFromGccDepsOutput(builder);
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

void CppSrc::setUpdateStatus()
{
    RealBTarget &rb = realBTargets[0];
    if (rb.updateStatus != UpdateStatus::UNCHECKED)
    {
        return;
    }

    if (node->fileType == file_type::not_found)
    {
        printErrorMessage(
            FORMAT("Source file does not exist.\nTarget: {}\nSource file: {}", target->name, node->filePath));
    }

    if (objectNode->fileType == file_type::not_found)
    {
        rb.updateStatus = UpdateStatus::UPDATE_NEEDED;
        return;
    }

    // `STACK_PMR_VECTOR` takes an element count, not a byte count, and reserves it before the first insertion.
    STACK_PMR_VECTOR(uint64_t, contentHashes, cachedHeaderFiles.size() + 2)
    contentHashes.emplace_back(commandHash);
    contentHashes.emplace_back(node->contentHash);
    for (const uint32_t nodeIndex : cachedHeaderFiles)
    {
        contentHashes.emplace_back(Node::getHalfNode(nodeIndex)->contentHash);
    }
    rb.cumulativeHash = rapidhash(contentHashes.data(), contentHashes.size() * 8);

    ObjectFile::setUpdateStatus();
}

bool CppSrc::isEventRegistered(Builder &builder)
{
    if (!selectiveBuild)
    {
        return false;
    }
    if (!refreshUpdateStatus())
    {
        return false;
    }

    STACK_PMR_STRING(cppFullCompileCommand, 64 * 1024)
    getCompileCommand(cppFullCompileCommand);
    if (dryRun)
    {
        cppFullCompileCommand += '\n';
        printMessage(cppFullCompileCommand);
        return false;
    }

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
            headerNode->doHashFile = true;
        }
        buildCacheUpdated = true;
        buildFooterUpdated = true;
    }

    STACK_PMR_STRING(outputStr, 64 * 1024)
    if (isConsole)
    {
        outputStr += getColorCode(ColorIndex::cyan);
    }

    if (run.output->empty())
    {
        outputStr += FORMAT("[{}/{}]C++Source {} {}\n", builder.updatedCount, builder.readyBTargetsSizeGoal,
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
    STACK_PMR_VECTOR(uint64_t, contentHashes, headerFiles.size() + 2)
    contentHashes.emplace_back(commandHash);
    contentHashes.emplace_back(node->contentHash);
    for (const Node *headerNode : headerFiles)
    {
        contentHashes.emplace_back(headerNode->lastWriteTime > initiationTime ? 0 : headerNode->contentHash);
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
        vector<uint64_t> contentHashes;
        contentHashes.reserve(headerFiles.size() + 2);
        contentHashes.emplace_back(commandHash);
        contentHashes.emplace_back(node->contentHash);
        for (const Node *headerNode : headerFiles)
        {
            contentHashes.emplace_back(headerNode->lastWriteTime > initiationTime ? 0 : headerNode->contentHash);
        }

        const uint64_t recomputedHash = rapidhash(contentHashes.data(), contentHashes.size() * 8);
        const path debugFile = target->myBuildDir->filePath + slashc + string("hashes") + toString(node->myId) + ".txt";
        if (std::ofstream out(debugFile, std::ios::app); out)
        {
            out << "commandHash:       " << commandHash << '\n';
            out << "node->contentHash: " << node->contentHash << '\n';
            size_t contentHashIndex = 2;
            for (const Node *headerNode : headerFiles)
            {
                out << "header " << (headerNode ? headerNode->filePath : "<null>")
                    << " hash=" << contentHashes[contentHashIndex++] << '\n';
            }
            out << "recomputedHash: " << recomputedHash << '\n';
            out << "storedHash:     " << rb.cumulativeHash << '\n';
        }

        if (recomputedHash != rb.cumulativeHash)
        {
            printErrorMessage(FORMAT("Build cache verification failed: content hash mismatch.\nTarget: {}\n"
                                     "Debug dump: {}\nRecomputed hash: {}\nCached hash: {}\nHeader count: {}",
                                     getPrintName(), debugFile.string(), recomputedHash, rb.cumulativeHash,
                                     headerFiles.size()));
        }
    }

    uint32_t bytesRead = 0;

    const uint32_t cachedHeaderFilesSize = readUint32(buildCache.data(), bytesRead);
    if (headerFiles.size() != cachedHeaderFilesSize)
    {
        printErrorMessage(FORMAT("Build cache verification failed: header count mismatch.\nTarget: {}\n"
                                 "Current count: {}\nCached count: {}",
                                 getPrintName(), headerFiles.size(), cachedHeaderFilesSize));
    }

    for (uint32_t i = 0; i < cachedHeaderFilesSize; ++i)
    {
        const Node *cachedNode = readHalfNode(buildCache.data(), bytesRead);
        if (!headerFiles.contains(cachedNode))
        {
            printErrorMessage(FORMAT("Build cache verification failed: cached header is not a current dependency.\n"
                                     "Target: {}\nHeader: {}\nCache index: {}",
                                     getPrintName(), cachedNode ? cachedNode->filePath : "<null>", i));
        }
    }

    verifyBTargetHeader(buildCache, bytesRead);
    if (buildCache.size() != bytesRead)
    {
        printErrorMessage(FORMAT("Build cache verification failed: entry size mismatch.\nTarget: {}\n"
                                 "Entry size: {} bytes\nBytes consumed: {}",
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
        const string_view configCache = bTargetCaches[cacheIndex].configCache;
        const char *ptr = configCache.data();

        if (!isImpl)
        {
            interfaceNode = readHalfNode(ptr, bytesRead);
            logicalName = readStringView(ptr, bytesRead);
        }

        if (!isHU)
        {
            objectNode = readHalfNode(ptr, bytesRead);
            target->imodNames.emplace(logicalName, this);
        }
        else
        {
            isReqHu = readBool(ptr, bytesRead);
            isUseReqHu = readBool(ptr, bytesRead);

            const uint32_t composingHeadersSize = readUint32(ptr, bytesRead);

            for (uint32_t j = 0; j < composingHeadersSize; ++j)
            {
                string_view headerFileName = readStringView(ptr, bytesRead);
                composingNames.emplace_back(headerFileName);
                if (target->useIPC)
                {
                    Node *headerNode = readHalfNode(ptr, bytesRead);
                    composingHeaders.emplace(headerFileName, headerNode);
                }
                else
                {
                    composingHeaders.emplace(headerFileName, nullptr);
                }

                if (isReqHu)
                {
                    target->reqHeaderNameMapping.emplace(headerFileName,
                                                         HfOrCppMod(this, FileType::HEADER_UNIT, target->isSystem));
                }

                if (isUseReqHu)
                {
                    const auto &[it, ok] =
                        target->configuration->headerNameMapping.emplace(headerFileName, vector<HfOrCppMod>{});
                    it->second.emplace_back(target->cacheIndex, this, FileType::HEADER_UNIT, target->isSystem);
                }
            }

            if (isReqHu)
            {
                target->reqHeaderNameMapping.emplace(logicalName,
                                                     HfOrCppMod(this, FileType::HEADER_UNIT, target->isSystem));
            }

            if (isUseReqHu)
            {
                const auto &[it, ok] =
                    target->configuration->headerNameMapping.emplace(logicalName, vector<HfOrCppMod>{});
                it->second.emplace_back(target->cacheIndex, this, FileType::HEADER_UNIT, target->isSystem);
            }
        }

        if (bytesRead != configCache.size())
        {
            HMAKE_HMAKE_INTERNAL_ERROR
        }
    }

    uint32_t bytesRead = 0;

    const string_view buildCache = bTargetCaches[cacheIndex].getBuildCache();
    const char *ptr = buildCache.data();

    // headerStatusChanged
    if (readBool(ptr, bytesRead))
    {
        rb.updateStatus = UpdateStatus::UPDATE_NEEDED;
    }

    const_cast<Node *>(node)->doHashFile = true;

    const uint32_t headerFilesSize = readUint32(ptr, bytesRead);

    cachedHeaderFiles = span{reinterpret_cast<const uint32_t *>(ptr + bytesRead), headerFilesSize};
    bytesRead += headerFilesSize * 4;
    for (const uint32_t headerNode : cachedHeaderFiles)
    {
        Node::getHalfNode(headerNode)->doHashFile = true;
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
        interfaceNode->doStatFile = true;
    }

    if (!isHU)
    {
        objectNode->doStatFile = true;
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
        printErrorMessage(FORMAT("Could not map the shared-memory BMI file.\nTarget: {}\nSource file: {}\n"
                                 "BMI file: {}\nSystem error: {}",
                                 target->name, node->filePath, interfaceNode->filePath, r.error()));
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

    for (const uint32_t &dep : cachedDeps)
    {
        if (CppMod *cppMod = static_cast<CppMod *>(bTargetCaches[dep].bTarget);
            allCppModDeps.emplace(CppModWithDirect(cppMod, true)).second)
        {
            cppMod->populateAllDeps();
            for (const CppModWithDirect &transitive : cppMod->allCppModDeps)
            {

                allCppModDeps.emplace(transitive.getPointer(), false);
            }
        }
    }
}

void CppMod::makeAndSendBTCModule(CppMod &mod)
{
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
        dep.logicalNames.emplace_back(modDep->logicalName);

        btcModule.modDeps.emplace_back(std::move(dep));
    }

    if (const auto &r2 = ipcManager->sendMessage(btcModule); !r2)
    {
        printErrorMessage(FORMAT("Could not send a module dependency to the compiler.\nTarget: {}\n"
                                 "Compiling file: {}\nDependency module: {}\nIPC error: {}",
                                 target->name, node->filePath, mod.node->filePath, r2.error()));
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
    hu.makeMemoryFileMapping();
    hu.populateAllDeps();

    STACK_PMR_STRING(toBeSend, 64 * 1024)

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
    writeUint32(toBeSend, hu.composingNames.size() + 1);
    writeStringView(toBeSend, hu.logicalName);
    for (const string_view &inclName : hu.composingNames)
    {
        writeStringView(toBeSend, inclName);
    }

    // index of the place-holder size of huDeps
    const uint32_t placeHolderIndex = toBeSend.size();

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
        writeUint32(toBeSend, modDep->composingNames.size() + 1);
        writeStringView(toBeSend, modDep->logicalName);
        for (const string_view &str : modDep->composingNames)
        {
            writeStringView(toBeSend, str);
        }
    }
    *reinterpret_cast<uint32_t *>(&toBeSend[placeHolderIndex]) = count;
    toBeSend.append(P2978::delimiter, strlen(P2978::delimiter));

    if (const auto &r2 = ipcManager->writeInternal(toBeSend); !r2)
    {
        printErrorMessage(FORMAT("Could not send a header-unit dependency to the compiler.\nTarget: {}\n"
                                 "Compiling kind: {}\nCompiling file: {}\nHeader unit: {}\nIPC error: {}",
                                 target->name, type == CppModType::HEADER_UNIT ? "header unit" : "module",
                                 node->filePath, hu.node->filePath, r2.error()));
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

std::optional<HfOrCppMod> CppMod::findHfOrCppMod(const string_view headerName) const
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

        for (const vector<HfOrCppMod> &configHeaderFilesOrUnits = it->second;
             const HfOrCppMod &hfOrCppMod : configHeaderFilesOrUnits)
        {
            if (std::ranges::find(target->cachedReqDeps, hfOrCppMod.data.cppMod->target->cacheIndex) !=
                target->cachedReqDeps.end())
            {
                // this hfOrCppMod is provided by one of our dependency cpp-target.
                return hfOrCppMod;
            }
        }
    }

    return {};
}

bool CppMod::isEventRegistered(Builder &builder)
{
    // an optimization is to increase/decrease the activeEventCount for less stack.
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }

    if (!selectiveBuild)
    {
        return false;
    }

    RealBTarget &rb = realBTargets[0];
    if (waitingFor)
    {
        return resumeAfterDependency(builder);
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

    if (!refreshUpdateStatus())
    {
        return false;
    }

    STACK_PMR_STRING(cppFullCompileCommand, 64 * 1024)
    getCompileCommand(cppFullCompileCommand, target->useIPC ? CommandType::USE_IPC : CommandType::CONVENTIONAL, "");
    if (dryRun)
    {
        cppFullCompileCommand += '\n';
        printMessage(cppFullCompileCommand);
        return false;
    }

    if (!target->useIPC)
    {
        run.startAsyncProcess(cppFullCompileCommand.c_str(), builder, this, false);
        return true;
    }

    isAllDepsPopulated = true;

    run.startAsyncProcess(cppFullCompileCommand.c_str(), builder, this, true);
    ipcManager = new IPCManagerBS(run.writePipe);

    return true;
}

bool CppMod::resumeAfterDependency(Builder &builder)
{
    if (!waitingFor)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }

    RealBTarget &rb = realBTargets[0];
    CppMod *completedDependency = waitingFor;
    waitingFor = nullptr;

    if (completedDependency->realBTargets[0].exitStatus != EXIT_SUCCESS)
    {
        run.killModuleProcess(builder);
        rb.exitStatus = EXIT_FAILURE;
        return false;
    }

    if (!refreshUpdateStatus())
    {
        run.killModuleProcess(builder);
        return false;
    }

    if (completedDependency->type == CppModType::HEADER_UNIT)
    {
        makeAndSendBTCNonModule(*completedDependency);
    }
    else
    {
        makeAndSendBTCModule(*completedDependency);
    }

    // The existing event registration remains active; the compiler can now issue its next nonempty CTB message.
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
            headerFile->doHashFile = true;
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
            printErrorMessage(FORMAT("Could not create the shared-memory BMI file.\nTarget: {}\nSource file: {}\n"
                                     "BMI file: {}\nSystem error: {}",
                                     target->name, node->filePath, interfaceNode->filePath, r2.error()));
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
    // Builder::readyBTargets.
    // 2) above string_view message can passed directly as part of output instead of first separating it out.
    // 3) Probably don't do colored output and copy from the Ninja. also take look for optimizing the header-file
    // parsing from msvc output and gcc .d files

    RealBTarget &rb = realBTargets[0];
    if (!target->useIPC)
    {
        // todo
        // command currently does not add .d file and that .d file must be passed as-well.
        // parseHeaderDeps(*run.output, builder);
        completeModuleCompilation(builder);
        return false;
    }

    // Builder reserves an empty view for EOF after reaping the compiler. CTB messages always contain at least their
    // one-byte request type, while dependency wakeups use resumeAfterDependency() and never enter this function.
    if (message.empty())
    {
        if (waitingFor)
        {
            printErrorMessage(FORMAT("Compiler exited while blocked on a dynamic dependency.\n"
                                     "This violates the module IPC protocol: a compiler waiting for a BTC response "
                                     "must keep its pipe open.\nTarget: {}\nCompiling file: {}\nWaited-on file: {}",
                                     target->name, node->filePath, waitingFor->node->filePath));
        }
        completeModuleCompilation(builder);
        return false;
    }

    if (waitingFor)
    {
        printErrorMessage(FORMAT("Compiler sent another IPC request before its dependency response.\n"
                                 "Target: {}\nCompiling file: {}\nWaited-on file: {}",
                                 target->name, node->filePath, waitingFor->node->filePath));
    }

    char buffer[320];
    P2978::CTB requestType;
    if (const auto &r = IPCManagerBS::receiveMessage(buffer, requestType, message); !r)
    {
        printErrorMessage(FORMAT("Could not receive a compiler IPC request.\nTarget: {}\nCompiling file: {}\n"
                                 "IPC error: {}",
                                 target->name, node->filePath, r.error()));
    }

    CppMod *found;

    if (requestType == P2978::CTB::NON_MODULE)
    {
        auto &[isHURequested, headerName] = reinterpret_cast<P2978::CTBNonModule &>(buffer);

        std::optional<HfOrCppMod> f = findHfOrCppMod(headerName);
        if (!f)
        {
            if (target->configuration->evaluate(UseConfigurationScope::YES))
            {
                printErrorMessageNoReturn(FORMAT("No file provides the requested header.\nConfiguration: {}\n"
                                                 "Target: {}\nCompiling file: {}\nRequested header: {}",
                                                 target->configuration->name, target->name, node->filePath,
                                                 headerName));
            }
            else
            {
                printErrorMessageNoReturn(FORMAT("No file provides the requested header.\nTarget: {}\n"
                                                 "Compiling file: {}\nRequested header: {}\nDependencies: {}",
                                                 target->name, node->filePath, headerName,
                                                 target->getDependenciesString()));
            }

            run.killModuleProcess(builder);
            rb.exitStatus = EXIT_FAILURE;
            return false;
        }

        // Checking if this is a big header-unit with composing header-files. Composing headers should be
        // included in the big header with same logical-name as they are meant to be used in other files. So we
        // can use the same headerName to search whether we have a composing header specified. Otherwise, it
        // would be diagnosed as cyclic dependency.
        if (f->data.cppMod == this && !firstMessageSent)
        {
            if (const auto it = composingHeaders.find(headerName); it != composingHeaders.end())
            {
                f = HfOrCppMod(it->second, FileType::HEADER_FILE, false);
            }
        }

        if (f->type == FileType::HEADER_UNIT)
        {
            found = f->data.cppMod;
        }
        else
        {
            if (isHURequested)
            {
                printErrorMessage(FORMAT("Requested header unit was not found.\nTarget: {}\nCompiling file: {}\n"
                                         "Requested header unit: {}",
                                         target->name, node->filePath, headerName));
            }

            STACK_PMR_STRING(toBeSend, 64 * 1024)

            // BTCNonModule::isHeaderUnit
            writeBool(toBeSend, false);
            // BTCNonModule::isSystem
            writeBool(toBeSend, f->isSystem);
            const uint32_t placeHolderIndex = toBeSend.size();

            bool addedInComposingHeader = false;
            if (!firstMessageSent)
            {
                uint32_t count = 0;
                writeUint32(toBeSend, -1); // placeholder
                firstMessageSent = true;
                for (const auto &[str, composingNode] : composingHeaders)
                {
                    if (f->data.node == composingNode && headerName == str)
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
            writeStringView(toBeSend, f->data.node->filePath);
            toBeSend.push_back('\n');
            toBeSend.append(P2978::delimiter, strlen(P2978::delimiter));

            if (const auto &r2 = ipcManager->writeInternal(toBeSend); !r2)
            {
                printErrorMessage(FORMAT("Could not send a header dependency to the compiler.\nTarget: {}\n"
                                         "Compiling file: {}\nHeader file: {}\nIPC error: {}",
                                         target->name, node->filePath, f->data.node->filePath, r2.error()));
            }

            if (!addedInComposingHeader)
            {
                if (!composingHeaders.emplace(headerName, f->data.node).second)
                {
                    printErrorMessage(FORMAT("Compiler requested the same composing header more than once.\n"
                                             "Target: {}\nCompiling file: {}\nHeader file: {}\nLogical name: {}",
                                             target->name, node->filePath, f->data.node->filePath, headerName));
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
                printErrorMessage(FORMAT("No file provides the requested module partition.\nTarget: {}\n"
                                         "Compiling file: {}\nRequested module: {}",
                                         target->name, node->filePath, moduleName));
            }
            else
            {
                printErrorMessage(FORMAT("No file provides the requested module.\nTarget: {}\nCompiling file: {}\n"
                                         "Requested module: {}\nDependencies: {}",
                                         target->name, node->filePath, moduleName, target->getDependenciesString()));
            }
        }
    }

    const auto &[it, ok] = allCppModDeps.emplace(found, true);

    if (!ok)
    {
        printErrorMessage(FORMAT("Internal error: compiler requested a module that was already sent.\nTarget: {}\n"
                                 "Compiling file: {}\nModule file: {}\nLogical name: {}",
                                 target->name, node->filePath, found->node->filePath, found->logicalName));
    }

    RealBTarget &foundRb = found->realBTargets[0];

    // Record every runtime provider before re-evaluation, including one that already completed. This ensures a newly
    // discovered provider that changed remains a rebuild reason and prevents an incorrect in-flight cutoff.
    foundRb.dependents.emplace(&rb, RelationType::FULL, BTargetType::CPP_MOD);
    const bool insertSucceeded = rb.dependencies.emplace(&foundRb, RelationType::FULL, BTargetType::CPP_MOD).second;

    if (!foundRb.isCompleted)
    {
        waitingFor = found;
        if (insertSucceeded)
        {
            ++rb.dependenciesSize;
        }

        // if its dependenciesSize is zero, it means that it is already in the list. We just bring it to the front.
        if (!found->isScheduled && foundRb.dependenciesSize == 0)
        {
            found->isScheduled = true;
            // Old index is reset and then we re-add
            builder.readyBTargets.array[foundRb.insertionIndex].value = nullptr;
            uint32_t insertionIndex = 0;
            builder.readyBTargets.emplace(&foundRb, insertionIndex);
            foundRb.insertionIndex = insertionIndex; // not needed probably
        }
        // This process is going to idle. Build-system will automatically decrement when it launches a new process.
        ++builder.availableProcessSlots;
        return true;
    }

    if (foundRb.exitStatus != EXIT_SUCCESS)
    {
        run.killModuleProcess(builder);
        rb.exitStatus = EXIT_FAILURE;
        return false;
    }

    if (!refreshUpdateStatus())
    {
        run.killModuleProcess(builder);
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
    STACK_PMR_STRING(outputStr, 64 * 1024)
    if (isConsole)
    {
        outputStr += getColorCode(type == CppModType::HEADER_UNIT ? ColorIndex::hot_pink : ColorIndex::magenta);
    }

    if (output.empty())
    {
        outputStr += FORMAT("[{}/{}]C++{} {} {}", builder.updatedCount, builder.readyBTargetsSizeGoal,
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
        printErrorMessage(FORMAT("A module implementation unit must be a C++ source file.\nTarget: {}\n"
                                 "Module file: {}\nDetected source type: {}",
                                 target->name, node->filePath, static_cast<uint8_t>(sourceType)));
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
    if (commandType != CommandType::CONVENTIONAL)
    {
        // so the compiler do not launch a new process to pre-process the file
        compileCommand += "-fno-crash-diagnostics ";
    }

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

void CppMod::setUpdateStatus()
{
    RealBTarget &rb = realBTargets[0];
    if (rb.updateStatus != UpdateStatus::UNCHECKED)
    {
        return;
    }
    rb.reasonForUpdate = nullptr;

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

        printErrorMessage(FORMAT("Required compilation input does not exist.\nTarget: {}\nInput kind: {}\nPath: {}",
                                 target->name, str, node->filePath));
    }

    rb.updateStatus = UpdateStatus::UPDATE_NEEDED;

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

    for (const uint32_t cahceIndex : cachedDeps)
    {
        CppMod *cppMod = static_cast<CppMod *>(bTargetCaches[cahceIndex].bTarget);

        // Can happen because the export-name or the include-name got mapped to a different file in the same target.
        if (!cppMod)
        {
            return;
        }

        const RealBTarget *depRb = &cppMod->realBTargets[0];
        if (depRb->updateStatus == UpdateStatus::UNCHECKED)
        {
            cppMod->setUpdateStatus();
        }

        if (depRb->updateStatus == UpdateStatus::UPDATE_NEEDED)
        {
            rb.reasonForUpdate = cppMod;
            return;
        }

        if (depRb->completionTime > rb.completionTime)
        {
            rb.reasonForUpdate = cppMod;
            return;
        }
    }

    rb.updateStatus = UpdateStatus::UNCHECKED;

    // command-hash + source-hash + cachedHeaderFiles
    STACK_PMR_VECTOR(uint64_t, contentHashes, cachedHeaderFiles.size() + 2)
    contentHashes.emplace_back(commandHash);
    contentHashes.emplace_back(node->contentHash);
    for (const uint32_t nodeIndex : cachedHeaderFiles)
    {
        contentHashes.emplace_back(Node::getHalfNode(nodeIndex)->contentHash);
    }
    rb.cumulativeHash = rapidhash(contentHashes.data(), contentHashes.size() * 8);

    ObjectFile::setUpdateStatus();
}

void CppMod::generateStandAloneCommand()
{
    if (target->configuration->evaluate(StandAloneCommand::YES))
    {
        if (const RealBTarget &rb = realBTargets[0]; rb.updateStatus == UpdateStatus::UPDATE_NEEDED)
        {
            path scriptDirectory = target->myBuildDir->filePath;
            scriptDirectory /= node->getFileName() + toString(node->myId);
            std::filesystem::create_directory(scriptDirectory);
            string scriptContents =
                FORMAT("#!/bin/bash\n\nset -x\n\n# This script compiles {}. Run it in the build-dir with same "
                       "name as current build-dir.\n\n",
                       node->filePath);
            flat_hash_set<string> createdDirs;
            cppStandAloneCommand(createdDirs, scriptContents, scriptDirectory.string(), true);
            std::ofstream(scriptDirectory / "script.sh") << scriptContents;
        }
    }
}

void CppMod::cppStandAloneCommand(flat_hash_set<string> &createdDirs, string &scriptContents, const string &scriptDir,
                                  bool direct)
{
    if (node->filePath.contains("one"))
    {
        bool breakpoint = true;
    }
    if (direct)
    {
        uint32_t a = realBTargets[0].dependencies.size();
        btree_set<BTarget *, IndexInTopologicalSortComparatorRoundZero> allTransitiveDeps;
        realBTargets[0].getAllWaitDepsTopological(allTransitiveDeps);
        uint32_t b = allTransitiveDeps.size();

        // We call for every BTarget so if there are custom code generation steps, then those could be added to the
        // script file as well.
        for (auto it = allTransitiveDeps.rbegin(); it != allTransitiveDeps.rend(); ++it)
        {
            (*it)->cppStandAloneCommand(createdDirs, scriptContents, scriptDir, false);
        }
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
        STACK_PMR_STRING(cppFullCompileCommand, 64 * 1024)
        getCompileCommand(cppFullCompileCommand,
                          target->useIPC ? CommandType::USE_IPC_MOCK_FILE : CommandType::CONVENTIONAL, "");

        scriptContents += cppFullCompileCommand;
        scriptContents.push_back('\n');
        return;
    }

    const string mockFilePath = (path(scriptDir) / FORMAT("mock-file{}.bin", id)).string();
    {
        // New scope for mock-file.bin
        STACK_PMR_STRING(mockFileContents, 256 * 1024)

        flat_hash_set<string> ignoreNames;

        uint32_t count = 0;
        writeUint32(mockFileContents, count);

        for (const CppModWithDirect &cppModWithDirect : allCppModDeps)
        {
            CppMod *cppMod = cppModWithDirect.getPointer();
            auto writeLogicalName = [&](const string_view &logicalName) {
                ignoreNames.emplace(logicalName);
                ++count;
                writeStringView(mockFileContents, logicalName);
                writeStringView(mockFileContents, cppMod->interfaceNode->filePath);
                mockFileContents.push_back('\0');
                FileType file;
                if (cppMod->type == CppModType::HEADER_UNIT)
                {
                    file = FileType::HEADER_UNIT;
                }
                else
                {
                    file = FileType::MODULE;
                }
                writeUint8(mockFileContents, static_cast<uint8_t>(file));
                writeBool(mockFileContents, cppMod->target->isSystem);
            };
            writeLogicalName(cppMod->logicalName);
            for (const string_view &inclName : cppMod->composingNames)
            {
                writeLogicalName(inclName);
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

    STACK_PMR_STRING(cppFullCompileCommand, 64 * 1024)
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
        writeStringView(buffer, logicalName);
    }

    if (!isHU)
    {
        objectNode =
            Node::getNode(target->myBuildDir->filePath + slashc + node->getFileName() + fileNumber + ".o", true, true);
        writeNode(buffer, objectNode);
    }
    else
    {
        writeBool(buffer, isReqHu);
        writeBool(buffer, isUseReqHu);
        writeUint32(buffer, composingHeaders.size());
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
            printErrorMessage(FORMAT("Configuration cache verification failed: BMI path mismatch.\nTarget: {}\n"
                                     "Current path: {}\nCached path: {}",
                                     getPrintName(), interfaceNode ? interfaceNode->filePath : "<null>",
                                     cachedInterfaceNode ? cachedInterfaceNode->filePath : "<null>"));
        }

        if (!isHU)
        {
            const string_view cachedLogicalName = readStringView(configCache.data(), bytesRead);
            if (logicalName.empty() || logicalName != cachedLogicalName)
            {
                printErrorMessage(FORMAT("Configuration cache verification failed: logical name mismatch.\n"
                                         "Target: {}\nCurrent name: {}\nCached name: {}",
                                         getPrintName(), logicalName, cachedLogicalName));
            }
        }
    }

    if (!isHU)
    {
        const Node *cachedObjectNode = readHalfNode(configCache.data(), bytesRead);
        if (objectNode != cachedObjectNode)
        {
            printErrorMessage(FORMAT("Configuration cache verification failed: object path mismatch.\nTarget: {}\n"
                                     "Current path: {}\nCached path: {}",
                                     getPrintName(), objectNode ? objectNode->filePath : "<null>",
                                     cachedObjectNode ? cachedObjectNode->filePath : "<null>"));
        }
    }
    else
    {
        const bool cachedIsReqHu = readBool(configCache.data(), bytesRead);
        if (isReqHu != cachedIsReqHu)
        {
            printErrorMessage(FORMAT("Configuration cache verification failed: private header-unit flag mismatch.\n"
                                     "Target: {}\nCurrent value: {}\nCached value: {}",
                                     getPrintName(), isReqHu, cachedIsReqHu));
        }

        const bool cachedIsUseReqHu = readBool(configCache.data(), bytesRead);
        if (isUseReqHu != cachedIsUseReqHu)
        {
            printErrorMessage(FORMAT("Configuration cache verification failed: interface header-unit flag mismatch.\n"
                                     "Target: {}\nCurrent value: {}\nCached value: {}",
                                     getPrintName(), isUseReqHu, cachedIsUseReqHu));
        }

        const uint32_t cachedComposingHeadersSize = readUint32(configCache.data(), bytesRead);
        if (composingHeaders.size() != cachedComposingHeadersSize)
        {
            printErrorMessage(FORMAT("Configuration cache verification failed: composing-header count mismatch.\n"
                                     "Target: {}\nCurrent count: {}\nCached count: {}",
                                     getPrintName(), composingHeaders.size(), cachedComposingHeadersSize));
        }

        for (uint32_t i = 0; i < cachedComposingHeadersSize; ++i)
        {
            const string_view cachedHeaderName = readStringView(configCache.data(), bytesRead);
            const auto it = composingHeaders.find(cachedHeaderName);
            if (it == composingHeaders.end())
            {
                printErrorMessage(
                    FORMAT("Configuration cache verification failed: cached composing header is missing.\n"
                           "Target: {}\nHeader name: {}\nCache index: {}",
                           getPrintName(), cachedHeaderName, i));
            }

            if (target->useIPC)
            {
                const Node *cachedHeaderNode = readHalfNode(configCache.data(), bytesRead);
                if (it != composingHeaders.end() && it->second != cachedHeaderNode)
                {
                    printErrorMessage(
                        FORMAT("Configuration cache verification failed: composing-header path mismatch.\n"
                               "Target: {}\nHeader name: {}\nCurrent path: {}\nCached path: {}",
                               getPrintName(), cachedHeaderName, it->second ? it->second->filePath : "<null>",
                               cachedHeaderNode ? cachedHeaderNode->filePath : "<null>"));
                }
            }
        }
    }

    if (configCache.size() != bytesRead)
    {
        printErrorMessage(FORMAT("Configuration cache verification failed: entry size mismatch.\nTarget: {}\n"
                                 "Entry size: {} bytes\nBytes consumed: {}",
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

    // command-hash + source-hash + container-size
    STACK_PMR_VECTOR(uint64_t, contentHashes, (target->useIPC ? composingHeaders.size() : headerFiles.size()) + 2)
    contentHashes.emplace_back(commandHash);
    contentHashes.emplace_back(node->contentHash);

    if (target->useIPC)
    {
        for (const auto &[includeName, headerNode] : composingHeaders)
        {
            if (headerNode->lastWriteTime > initiationTime)
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
            if (headerNode->lastWriteTime > initiationTime)
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
                if (headerNode->lastWriteTime > initiationTime)
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
                if (headerNode->lastWriteTime > initiationTime)
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
                printErrorMessage(FORMAT("Build cache verification failed: content hash mismatch.\nTarget: {}\n"
                                         "Recomputed hash: {}\nCached hash: {}",
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
            printErrorMessage(FORMAT("Build cache verification failed: header classification changed during build.\n"
                                     "Target: {}",
                                     getPrintName()));
        }
    }

    if (target->useIPC)
    {
        const uint32_t cachedComposingHeadersSize = readUint32(buildCache.data(), bytesRead);
        if (composingHeaders.size() != cachedComposingHeadersSize)
        {
            printErrorMessage(FORMAT("Build cache verification failed: composing-header count mismatch.\nTarget: {}\n"
                                     "Current count: {}\nCached count: {}",
                                     getPrintName(), composingHeaders.size(), cachedComposingHeadersSize));
        }

        for (uint32_t i = 0; i < cachedComposingHeadersSize; ++i)
        {
            const Node *cachedNode = readHalfNode(buildCache.data(), bytesRead);
            const auto it = std::find_if(composingHeaders.begin(), composingHeaders.end(),
                                         [cachedNode](const auto &kv) { return kv.second == cachedNode; });
            if (it == composingHeaders.end())
            {
                printErrorMessage(FORMAT("Build cache verification failed: cached composing header is missing.\n"
                                         "Target: {}\nHeader path: {}\nCache index: {}",
                                         getPrintName(), cachedNode ? cachedNode->filePath : "<null>", i));
            }
        }
    }
    else
    {
        const uint32_t cachedHeaderFilesSize = readUint32(buildCache.data(), bytesRead);
        if (headerFiles.size() != cachedHeaderFilesSize)
        {
            printErrorMessage(FORMAT("Build cache verification failed: header count mismatch.\nTarget: {}\n"
                                     "Current count: {}\nCached count: {}",
                                     getPrintName(), headerFiles.size(), cachedHeaderFilesSize));
        }

        for (uint32_t i = 0; i < cachedHeaderFilesSize; ++i)
        {
            const Node *cachedNode = readHalfNode(buildCache.data(), bytesRead);
            if (!headerFiles.contains(cachedNode))
            {
                printErrorMessage(FORMAT("Build cache verification failed: cached header is not a current dependency.\n"
                                         "Target: {}\nHeader path: {}\nCache index: {}",
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
                printErrorMessage(FORMAT("Build cache verification failed: dependency cache index mismatch.\n"
                                         "Target: {}\nDependency position: {}\nCurrent index: {}\nCached index: {}",
                                         getPrintName(), count, cppModDirect.getPointer()->cacheIndex,
                                         cachedCacheIndex));
            }
            ++count;
        }
    }

    if (count != cachedDirectDepsCount)
    {
        printErrorMessage(FORMAT("Build cache verification failed: direct-dependency count mismatch.\nTarget: {}\n"
                                 "Current count: {}\nCached count: {}",
                                 getPrintName(), count, cachedDirectDepsCount));
    }

    verifyBTargetHeader(buildCache, bytesRead);

    if (buildCache.size() != bytesRead)
    {
        printErrorMessage(FORMAT("Build cache verification failed: entry size mismatch.\nTarget: {}\n"
                                 "Entry size: {} bytes\nBytes consumed: {}",
                                 getPrintName(), buildCache.size(), bytesRead));
    }
}
