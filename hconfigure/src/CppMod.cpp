
#include "CppMod.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "Configuration.hpp"
#include "CppTarget.hpp"
#include "IPCManagerCompiler.hpp"
#include "rapidhash/rapidhash.h"

#include <rapidjson/document.h>

#include <cctype>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <memory_resource>
#include <thread>
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

        uint64_t bytesRead = 0;
        const string_view configCache = bTargetCaches[cacheIndex].configCache;
        objectNodes.emplace_back(readHalfNode(configCache.data(), bytesRead));

        if (4 != configCache.size())
        {
            HMAKE_HMAKE_INTERNAL_ERROR
        }
    }

    uint64_t bytesRead = 0;

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

    objectNodes.front()->doStatFile = true;
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
        compileCommand += "-c /nologo ";
        if (target->configuration->msvcHeaderDependencyMode == MSVCHeaderDependencyMode::DEPENDENCY_FILE)
        {
            compileCommand += "/sourceDependencies \"" + objectNodes.front()->filePath + ".json\" ";
        }
        else
        {
            compileCommand += "/showIncludes ";
        }
        compileCommand += "/TP \"" + node->filePath + "\" /Fo\"" + objectNodes.front()->filePath + "\"";
    }
    else if (compiler.bTFamily == BTFamily::GCC)
    {
        compileCommand += "-c -MMD \"" + node->filePath + "\" -o \"" + objectNodes.front()->filePath + "\"";
    }
}

namespace
{
Node *dependencyNode(const string_view dependency, const path &workingDirectory, const Node *compiledSource,
                     const bool excludeHeadersInConfigureNode)
{
    if (dependency.empty())
    {
        return nullptr;
    }

    Node *node;
    if (Node::isAbsolute(dependency))
    {
        node = Node::getHalfNodeNonNormalized(dependency);
    }
    else
    {
        // Both callers supply an absolute working directory, so dependency normalization stays purely lexical.
        string normalized = (workingDirectory / path{dependency}).lexically_normal().string();
        lowerCaseOnWindows(normalized.data(), normalized.size());
        node = Node::getHalfNode(normalized);
    }

    if (node == compiledSource ||
        (excludeHeadersInConfigureNode && isPathInDirectory(node->filePath, configureNode->filePath)))
    {
        return nullptr;
    }
    return node;
}

flat_hash_set<Node *> parseShowIncludes(string &output, const bool isClang, const bool collectHeaders,
                                        const path &workingDirectory, const Node *compiledSource,
                                        const bool excludeHeadersInConfigureNode)
{
    flat_hash_set<Node *> dependencies;
    constexpr string_view includeFileNote = "Note: including file:";
    const uint64_t outputSize = output.size();
    uint64_t readOffset = 0;
    if (collectHeaders && !isClang)
    {
        const uint64_t firstLineEnd = output.find('\n');
        if (firstLineEnd == string::npos)
        {
            return dependencies;
        }
        readOffset = firstLineEnd + 1;
    }

    char *const data = output.data();
    uint64_t writeOffset = 0;
    while (readOffset < outputSize)
    {
        const char *const newline =
            static_cast<const char *>(std::memchr(data + readOffset, '\n', outputSize - readOffset));
        const uint64_t nextOffset = newline == nullptr ? outputSize : static_cast<uint64_t>(newline - data) + 1;
        const string_view line(data + readOffset, nextOffset - readOffset);
        const uint64_t notePosition = line.find(includeFileNote);

        if (notePosition == string_view::npos)
        {
            const uint64_t lineSize = nextOffset - readOffset;
            if (writeOffset != readOffset)
            {
                std::memmove(data + writeOffset, data + readOffset, lineSize);
            }
            writeOffset += lineSize;
        }
        else if (collectHeaders)
        {
            uint64_t headerStart = notePosition + includeFileNote.size();
            while (headerStart < line.size() && (line[headerStart] == ' ' || line[headerStart] == '\t'))
            {
                ++headerStart;
            }
            uint64_t headerEnd = line.size();
            while (headerEnd > headerStart && (line[headerEnd - 1] == '\n' || line[headerEnd - 1] == '\r' ||
                                               line[headerEnd - 1] == ' ' || line[headerEnd - 1] == '\t'))
            {
                --headerEnd;
            }
            if (headerStart != headerEnd)
            {
                const string_view headerView(data + readOffset + headerStart, headerEnd - headerStart);
                if (Node *header = dependencyNode(headerView, workingDirectory, compiledSource,
                                                  excludeHeadersInConfigureNode))
                {
                    dependencies.emplace(header);
                }
            }
        }
        readOffset = nextOffset;
    }
    output.resize(writeOffset);
    return dependencies;
}

flat_hash_set<Node *> parseMakeDependencies(const path &dependencyFile, const path &workingDirectory,
                                            const Node *compiledSource, const bool excludeHeadersInConfigureNode)
{
    flat_hash_set<Node *> dependencies;
    STACK_PMR_STRING(contents, 256 * 1024)
    fileToString(dependencyFile.string(), contents);
    uint64_t writeOffset = 0;
    const uint64_t contentStart = contents.starts_with("\xef\xbb\xbf") ? 3 : 0;
    for (uint64_t index = contentStart; index < contents.size(); ++index)
    {
        if (contents[index] == '\\' && index + 1 < contents.size() &&
            (contents[index + 1] == '\n' || contents[index + 1] == '\r'))
        {
            ++index;
            if (contents[index] == '\r' && index + 1 < contents.size() && contents[index + 1] == '\n')
            {
                ++index;
            }
            contents[writeOffset++] = ' ';
            continue;
        }
        contents[writeOffset++] = contents[index];
    }
    contents.resize(writeOffset);

    uint64_t colon = string::npos;
    bool escaped = false;
    for (uint64_t index = 0; index < contents.size(); ++index)
    {
        if (escaped)
        {
            escaped = false;
            continue;
        }
        if (contents[index] == '\\')
        {
            escaped = true;
            continue;
        }
        if (contents[index] == ':' &&
            (index + 1 == contents.size() || std::isspace(static_cast<unsigned char>(contents[index + 1]))))
        {
            colon = index;
            break;
        }
    }
    if (colon == string::npos)
    {
        printErrorMessage(FORMAT("Malformed Make dependency file.\nFile: {}", dependencyFile.string()));
    }

    STACK_PMR_STRING(token, 1024)
    bool comment = false;
    const auto commit = [&]() {
        if (!token.empty())
        {
            if (Node *dependency =
                    dependencyNode(token, workingDirectory, compiledSource, excludeHeadersInConfigureNode))
            {
                dependencies.emplace(dependency);
            }
            token.clear();
        }
    };
    for (uint64_t index = colon + 1; index <= contents.size(); ++index)
    {
        const char character = index == contents.size() ? ' ' : contents[index];
        if (comment)
        {
            if (character == '\n' || character == '\r')
            {
                comment = false;
            }
            continue;
        }
        if (character == '#')
        {
            commit();
            comment = true;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(character)))
        {
            commit();
            continue;
        }
        if (character == '\\' && index + 1 < contents.size())
        {
            const char next = contents[index + 1];
            if (std::isspace(static_cast<unsigned char>(next)) || next == '#' || next == ':' || next == '\\')
            {
                token.push_back(next);
                ++index;
                continue;
            }
        }
        token.push_back(character);
    }
    return dependencies;
}

void collectSourceDependencyPaths(const rapidjson::Value &value, const string_view memberName,
                                  const path &workingDirectory, const Node *compiledSource,
                                  const bool excludeHeadersInConfigureNode, flat_hash_set<Node *> &dependencies)
{
    if (value.IsObject())
    {
        for (auto iterator = value.MemberBegin(); iterator != value.MemberEnd(); ++iterator)
        {
            collectSourceDependencyPaths(iterator->value,
                                         {iterator->name.GetString(), iterator->name.GetStringLength()},
                                         workingDirectory, compiledSource, excludeHeadersInConfigureNode,
                                         dependencies);
        }
        return;
    }
    if (value.IsArray())
    {
        for (const rapidjson::Value &element : value.GetArray())
        {
            if (element.IsString() && memberName == "Includes")
            {
                if (Node *dependency = dependencyNode({element.GetString(), element.GetStringLength()},
                                                      workingDirectory, compiledSource,
                                                      excludeHeadersInConfigureNode))
                {
                    dependencies.emplace(dependency);
                }
            }
            else
            {
                collectSourceDependencyPaths(element, memberName, workingDirectory, compiledSource,
                                             excludeHeadersInConfigureNode, dependencies);
            }
        }
        return;
    }
    if (value.IsString() && (memberName == "Source" || memberName == "Header" || memberName == "Path"))
    {
        if (Node *dependency = dependencyNode({value.GetString(), value.GetStringLength()}, workingDirectory,
                                              compiledSource, excludeHeadersInConfigureNode))
        {
            dependencies.emplace(dependency);
        }
    }
}

flat_hash_set<Node *> parseSourceDependencies(const path &dependencyFile, const path &workingDirectory,
                                              const Node *compiledSource,
                                              const bool excludeHeadersInConfigureNode)
{
    STACK_PMR_STRING(json, 256 * 1024)
    fileToString(dependencyFile.string(), json);
    char *const documentStart = json.data() + (json.starts_with("\xef\xbb\xbf") ? 3 : 0);
    rapidjson::Document document;
    document.ParseInsitu(documentStart);
    if (document.HasParseError() || !document.IsObject())
    {
        printErrorMessage(FORMAT("Malformed MSVC source-dependencies file.\nFile: {}\nByte offset: {}",
                                 dependencyFile.string(), document.GetErrorOffset()));
    }
    flat_hash_set<Node *> dependencies;
    collectSourceDependencyPaths(document, {}, workingDirectory, compiledSource, excludeHeadersInConfigureNode,
                                 dependencies);
    return dependencies;
}
} // namespace

flat_hash_set<Node *> CppSrc::parseHeaderDeps(string &output, const Compiler &compiler, const int exitStatus,
                                              const path &dependencyFile, const path &workingDirectory,
                                              const Node *compiledSource,
                                              const bool excludeHeadersInConfigureNode)
{
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        if (!dependencyFile.empty())
        {
            if (exitStatus != EXIT_SUCCESS)
            {
                return {};
            }
            return parseSourceDependencies(dependencyFile, workingDirectory, compiledSource,
                                           excludeHeadersInConfigureNode);
        }
        return parseShowIncludes(output, compiler.btSubFamily == BTSubFamily::CLANG, exitStatus == EXIT_SUCCESS,
                                 workingDirectory, compiledSource, excludeHeadersInConfigureNode);
    }
    if (exitStatus != EXIT_SUCCESS)
    {
        return {};
    }
    return parseMakeDependencies(dependencyFile, workingDirectory, compiledSource, excludeHeadersInConfigureNode);
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

    if (objectNodes.front()->fileType == file_type::not_found)
    {
        rb.updateStatus = UpdateStatus::UPDATE_NEEDED;
        return;
    }

    STACK_PMR_VECTOR(uint64_t, contentHashes, 4 * 1024)
    contentHashes.reserve(cachedHeaderFiles.size() + 2);
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

    const uint64_t responseFileThreshold = target->configuration->responseFileThreshold;
    if (responseFileThreshold != 0 && cppFullCompileCommand.size() > responseFileThreshold)
    {
        commandWithResponseFile(cppFullCompileCommand, objectNodes.front()->filePath + ".rsp", responseFileThreshold);
    }
    run.startAsyncProcess(cppFullCompileCommand.data(), builder, this, false);
    return true;
}

bool CppSrc::isEventCompleted(Builder &builder, string_view)
{
    const Compiler &compiler = target->configuration->compilerFeatures.compiler;
    path dependencyFile;
    if (compiler.bTFamily == BTFamily::GCC)
    {
        dependencyFile = objectNodes.front()->filePath;
        dependencyFile.replace_extension(".d");
    }
    else if (compiler.bTFamily == BTFamily::MSVC &&
             target->configuration->msvcHeaderDependencyMode == MSVCHeaderDependencyMode::DEPENDENCY_FILE)
    {
        dependencyFile = objectNodes.front()->filePath + ".json";
    }
    headerFiles = parseHeaderDeps(*run.output, compiler, realBTargets[0].exitStatus, dependencyFile,
                                  currentNode->filePath, node, true);

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

    STACK_PMR_STRING(outputStr, 4 * 1024)
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
    STACK_PMR_STRING(objectFile, 2 * 1024)
    objectFile.reserve(target->myBuildDir->filePath.size() + 1 + node->getFileName().size() + fileNumber.size() + 2);
    objectFile.assign(target->myBuildDir->filePath);
    objectFile += slashc;
    objectFile += node->getFileName();
    objectFile += fileNumber;
    objectFile += ".o";
    objectNodes.emplace_back(Node::getNode(objectFile, true, true));
    writeNode(buffer, objectNodes.front());
}

void CppSrc::writeBuildCacheAtConfigTime(string &buffer)
{
    // sizeof header-files
    writeUint32(buffer, 0);
}

void CppSrc::writeBuildCacheAtBuildTime(string &buffer)
{
    RealBTarget &rb = realBTargets[0];
    STACK_PMR_VECTOR(uint64_t, contentHashes, 4 * 1024)
    contentHashes.reserve(headerFiles.size() + 2);
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
        STACK_PMR_VECTOR(uint64_t, contentHashes, 4 * 1024)
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
            uint64_t contentHashIndex = 2;
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

    uint64_t bytesRead = 0;

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
        uint64_t bytesRead = 0;
        const string_view configCache = bTargetCaches[cacheIndex].configCache;
        const char *ptr = configCache.data();

        if (!isImpl)
        {
            interfaceNode = readHalfNode(ptr, bytesRead);
            logicalName = readStringView(ptr, bytesRead);
        }

        if (!isHU)
        {
            objectNodes.emplace_back(readHalfNode(ptr, bytesRead));
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

    uint64_t bytesRead = 0;

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
        objectNodes.front()->doStatFile = true;
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
    mod.makeMemoryFileMapping();
    mod.populateAllDeps();

    STACK_PMR_STRING(toBeSend, 64 * 1024)

    // BTCModule::requested
    writeStringView(toBeSend, mod.interfaceNode->filePath);
    toBeSend.push_back('\0');
    writeUint32(toBeSend, mod.interfaceFileSize);
    // BTCModule::isSystem
    writeBool(toBeSend, mod.target->isSystem);

    // BTCModule::modDeps. Patch the count after filtering dependencies already sent to this compiler.
    const uint64_t dependencyCountOffset = toBeSend.size();
    writeUint32(toBeSend, 0);
    uint32_t dependencyCount = 0;

    for (const CppModWithDirect &transitive : mod.allCppModDeps)
    {
        CppMod *modDep = transitive.getPointer();
        if (!allCppModDeps.emplace(modDep, false).second)
        {
            continue;
        }

        modDep->makeMemoryFileMapping();

        ++dependencyCount;
        // ModuleDep::isHeaderUnit
        writeBool(toBeSend, modDep->type == CppModType::HEADER_UNIT);
        // ModuleDep::file
        writeStringView(toBeSend, modDep->interfaceNode->filePath);
        toBeSend.push_back('\0');
        writeUint32(toBeSend, modDep->interfaceFileSize);
        // ModuleDep::isSystem
        writeBool(toBeSend, modDep->target->isSystem);
        // ModuleDep::logicalNames
        writeUint32(toBeSend, 1);
        writeStringView(toBeSend, modDep->logicalName);
    }

    memcpy(toBeSend.data() + dependencyCountOffset, &dependencyCount, sizeof(dependencyCount));
    toBeSend.append(P2978::delimiter, strlen(P2978::delimiter));
    run.writeReadExpected(toBeSend);
}

// For debugging purposes
P2978::BTCNonModule deserializeBTCNonModule(std::string_view buffer)
{
    P2978::BTCNonModule result;
    const char *ptr = buffer.data();
    uint64_t bytesRead = 0;

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
    const uint64_t placeHolderIndex = toBeSend.size();

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

        assert(count != static_cast<uint32_t>(-1));
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
    memcpy(toBeSend.data() + placeHolderIndex, &count, sizeof(count));
    toBeSend.append(P2978::delimiter, strlen(P2978::delimiter));

    run.writeReadExpected(toBeSend);
}

CppMod *CppMod::findModule(const string_view moduleName) const
{
    if (const auto it = target->imodNames.find(moduleName); it != target->imodNames.end())
    {
        return it->second;
    }

    if (!moduleName.contains(':'))
    {
        FOR_REQ_OBJECT_FILE_PRODUCERS(target, producer, dependency)
        {
            if (!dependency.isOpDependency() || !producer->isCppTarget)
            {
                continue;
            }
            auto *req = static_cast<CppTarget *>(producer);
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
            CppTarget *provider = hfOrCppMod.data.cppMod->target;
            FOR_REQ_OBJECT_FILE_PRODUCERS(target, producer, dependency)
            {
                if (producer == provider && dependency.isOpDependency())
                {
                    // this hfOrCppMod is provided by one of our dependency cpp-target.
                    return hfOrCppMod;
                }
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

    const uint64_t responseFileThreshold = target->configuration->responseFileThreshold;
    if (responseFileThreshold != 0 && cppFullCompileCommand.size() > responseFileThreshold)
    {
        const Node *compileOutput = objectNodes.empty() ? interfaceNode : objectNodes.front();
        commandWithResponseFile(cppFullCompileCommand, compileOutput->filePath + ".rsp", responseFileThreshold);
    }
    if (!target->useIPC)
    {
        run.startAsyncProcess(cppFullCompileCommand.data(), builder, this, false);
        return true;
    }

    isAllDepsPopulated = true;

    run.startAsyncProcess(cppFullCompileCommand.data(), builder, this, true);

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
        for (Node *headerFile : headerFiles)
        {
            headerFile->doHashFile = true;
        }
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
        const Compiler &compiler = target->configuration->compilerFeatures.compiler;
        const Node *compileOutput = objectNodes.empty() ? interfaceNode : objectNodes.front();
        path dependencyFile;
        if (compiler.bTFamily == BTFamily::GCC)
        {
            dependencyFile = compileOutput->filePath;
            dependencyFile.replace_extension(".d");
        }
        else if (target->configuration->msvcHeaderDependencyMode ==
                 MSVCHeaderDependencyMode::DEPENDENCY_FILE)
        {
            dependencyFile = compileOutput->filePath + ".json";
        }
        headerFiles = parseHeaderDeps(*run.output, compiler, rb.exitStatus, dependencyFile,
                                      currentNode->filePath, node, true);
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

    if (requestType == P2978::CTB::LAST_MESSAGE)
    {
        // TODO: Map compiler-created BMI shared-memory files and acknowledge them with BTC::LAST_MESSAGE.
        printErrorMessage(FORMAT("Compiler sent CTB::LAST_MESSAGE, but compiler-created BMI shared-memory files "
                                 "are not yet supported.\nTarget: {}\nCompiling file: {}",
                                 target->name, node->filePath));
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
            const uint64_t placeHolderIndex = toBeSend.size();

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

                    assert(count != static_cast<uint32_t>(-1));
                    ++count;

                    writeStringView(toBeSend, str);
                    // HeaderFile::filePath
                    writeStringView(toBeSend, composingNode->filePath);
                    toBeSend.push_back('\n');
                    // HeaderFile::isSystem
                    writeBool(toBeSend, target->isSystem);
                }
                memcpy(toBeSend.data() + placeHolderIndex, &count, sizeof(count));
            }
            else
            {
                writeUint32(toBeSend, 0);
            }

            // BTCNonModule::filePath
            writeStringView(toBeSend, f->data.node->filePath);
            toBeSend.push_back('\n');
            toBeSend.append(P2978::delimiter, strlen(P2978::delimiter));

            run.writeReadExpected(toBeSend);

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

    return true;
}

void CppMod::print(const Builder &builder, const string &output) const
{
    STACK_PMR_STRING(outputStr, 4 * 1024)
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
    const Compiler &compiler = target->configuration->compilerFeatures.compiler;
    if (compiler.bTFamily == BTFamily::MSVC && compiler.btSubFamily == BTSubFamily::CLANG)
    {
        if (type == CppModType::HEADER_UNIT)
        {
            compileCommand +=
                (target->isSystem ? "-fmodule-header=system /clang:-o\"" : "-fmodule-header=user /clang:-o\"") +
                interfaceNode->filePath + "\" " + useIPCsTR + "-x c++-header \"" + node->filePath + '\"';
        }
        else if (type == CppModType::PRIMARY_EXPORT || type == CppModType::PARTITION_EXPORT)
        {
            compileCommand += " -o \"" + objectNodes.front()->filePath + "\" " + useIPCsTR + "-c -x c++-module \"" +
                              node->filePath + "\" -fmodule-output=\"" + interfaceNode->filePath + '\"';
        }
        else
        {
            compileCommand +=
                "-o \"" + objectNodes.front()->filePath + "\" " + useIPCsTR + "-c /TP \"" + node->filePath + '\"';
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
    else if (compiler.bTFamily == BTFamily::GCC && compiler.btSubFamily == BTSubFamily::CLANG)
    {
        if (type == CppModType::HEADER_UNIT)
        {
            compileCommand += (target->isSystem ? "-fmodule-header=system -o\"" : "-fmodule-header=user -o\"") +
                              interfaceNode->filePath + "\" " + useIPCsTR + "-x c++-header \"" + node->filePath + '\"';
        }
        else if (type == CppModType::PRIMARY_EXPORT || type == CppModType::PARTITION_EXPORT)
        {
            compileCommand += " -o \"" + objectNodes.front()->filePath + "\" " + useIPCsTR + "-c -x c++-module \"" +
                              node->filePath + "\" -fmodule-output=\"" + interfaceNode->filePath + '\"';
        }
        else
        {
            compileCommand +=
                "-o \"" + objectNodes.front()->filePath + "\" " + useIPCsTR + "-c \"" + node->filePath + '\"';
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

    const Node *compileOutput = objectNodes.empty() ? interfaceNode : objectNodes.front();
    if (compiler.bTFamily == BTFamily::MSVC)
    {
        if (target->configuration->msvcHeaderDependencyMode == MSVCHeaderDependencyMode::DEPENDENCY_FILE)
        {
            compileCommand += " /sourceDependencies \"" + compileOutput->filePath + ".json\" ";
        }
        else
        {
            compileCommand += " /showIncludes ";
        }
    }
    else
    {
        path dependencyFile = compileOutput->filePath;
        dependencyFile.replace_extension(".d");
        compileCommand += " -MMD -MF \"" + dependencyFile.string() + "\" ";
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
        if (objectNodes.front()->fileType == file_type::not_found)
        {
            return;
        }
    }
    else
    {
        if (interfaceNode->fileType == file_type::not_found || objectNodes.front()->fileType == file_type::not_found)
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
    STACK_PMR_VECTOR(uint64_t, contentHashes, 256)
    contentHashes.reserve(cachedHeaderFiles.size() + 2);
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
            string scriptName(node->getFileName());
            scriptName += toString(node->myId);
            scriptDirectory /= scriptName;
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

        memcpy(mockFileContents.data(), &count, sizeof(count));
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
        STACK_PMR_STRING(interfaceFile, 2 * 1024)
        interfaceFile.reserve(target->myBuildDir->filePath.size() + 1 + node->getFileName().size() +
                              fileNumber.size() + 4);
        interfaceFile.assign(target->myBuildDir->filePath);
        interfaceFile += slashc;
        interfaceFile += node->getFileName();
        interfaceFile += fileNumber;
        interfaceFile += ".ifc";
        interfaceNode = Node::getNode(interfaceFile, true, true);
        writeNode(buffer, interfaceNode);
        writeStringView(buffer, logicalName);
    }

    if (!isHU)
    {
        STACK_PMR_STRING(objectFile, 2 * 1024)
        objectFile.reserve(target->myBuildDir->filePath.size() + 1 + node->getFileName().size() + fileNumber.size() +
                           2);
        objectFile.assign(target->myBuildDir->filePath);
        objectFile += slashc;
        objectFile += node->getFileName();
        objectFile += fileNumber;
        objectFile += ".o";
        objectNodes.emplace_back(Node::getNode(objectFile, true, true));
        writeNode(buffer, objectNodes.front());
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
    uint64_t bytesRead = 0;

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
        const Node *objectNode = objectNodes.empty() ? nullptr : objectNodes.front();
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
    STACK_PMR_VECTOR(uint64_t, contentHashes, 256)
    contentHashes.reserve((target->useIPC ? composingHeaders.size() : headerFiles.size()) + 2);
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

    const uint64_t currentSize = buffer.size();
    uint32_t count = 0;
    // placeholder for direct-deps count;
    writeUint32(buffer, 0);

    for (const CppModWithDirect &cppModDirect : allCppModDeps)
    {
        if (cppModDirect.isDirect())
        {
            assert(count != static_cast<uint32_t>(-1));
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
        STACK_PMR_VECTOR(uint64_t, contentHashes, 256)
        contentHashes.reserve(2 + (target->useIPC ? composingHeaders.size() : headerFiles.size()));
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
    uint64_t bytesRead = 0;

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

AdaptiveManager::AdaptiveManager(CppTarget *target_)
    : BTarget(target_->name + "/adaptive-unity",
              rapidhash_withSeed(&target_->cacheName, sizeof(target_->cacheName),
                                 0x4144415054495645ULL), // "ADAPTIVE"
              false, BTargetType::UNKNOWN),
      target(target_)
{
}

void AdaptiveManager::prepareWorkingSet()
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        return;
    }

    // postConfigurationSpecification() runs once per active configuration. Cache the source-control result while
    // allowing each call to add the adaptive nodes discovered by that configuration.
    static bool sourceControlQueried = false;
    static flat_hash_set<string> sourceControlPaths;

    flat_hash_map<string, const Node *> candidates;
    for (Configuration *configuration : allConfigurations)
    {
        for (CppTarget *cppTarget : configuration->cppTargets)
        {
            for (Node *node : cppTarget->adaptiveSourceNodes)
            {
                candidates.emplace(node->filePath, node);
            }
        }
    }
    if (candidates.empty())
    {
        return;
    }
    if (srcNode == nullptr)
    {
        printErrorMessage("Adaptive unity requires a project source root (`srcNode`).");
    }

    const auto markPath = [&](const string_view reportedPath, const bool relativeToSourceRoot) {
        path candidatePath(reportedPath);
        if (relativeToSourceRoot)
        {
            candidatePath = path(srcNode->filePath) / candidatePath;
        }
        string normalized = candidatePath.lexically_normal().string();
        lowerCaseOnWindows(normalized.data(), normalized.size());
        sourceControlPaths.emplace(std::move(normalized));
    };

    if (!sourceControlQueried)
    {
        sourceControlQueried = true;
        if (adaptiveBuildWorkingSetProvider != WorkingSetProvider::NONE)
        {
            if (adaptiveBuildWorkingSetProvider == WorkingSetProvider::GIT)
            {
                const string commandLine =
                    "git -C " + addQuotes(srcNode->filePath) + " status --porcelain=v1 -z --untracked-files=all -- .";
                const auto result = RunCommand::runProcess(commandLine.c_str());
                if (result.exitStatus != EXIT_SUCCESS)
                {
                    printErrorMessage(
                        FORMAT("Could not query Git for the adaptive-unity working set.\nSource root: {}\n{}",
                               srcNode->filePath, result.output));
                }

                const string &output = result.output;
                uint64_t position = 0;
                while (position < output.size())
                {
                    const uint64_t end = output.find('\0', position);
                    const uint64_t tokenEnd = end == string::npos ? output.size() : end;
                    const string_view token(output.data() + position, tokenEnd - position);
                    if (token.size() >= 3)
                    {
                        const bool renameOrCopy =
                            token[0] == 'R' || token[1] == 'R' || token[0] == 'C' || token[1] == 'C';
                        markPath(token.substr(3), true);
                        position = tokenEnd + (end == string::npos ? 0 : 1);
                        if (renameOrCopy && position < output.size())
                        {
                            const uint64_t oldEnd = output.find('\0', position);
                            const uint64_t oldTokenEnd = oldEnd == string::npos ? output.size() : oldEnd;
                            markPath(string_view(output.data() + position, oldTokenEnd - position), true);
                            position = oldTokenEnd + (oldEnd == string::npos ? 0 : 1);
                        }
                        continue;
                    }
                    position = tokenEnd + (end == string::npos ? 0 : 1);
                }
            }
            else
            {
                const auto result = RunCommand::runProcess("p4 -ztag opened");
                if (result.exitStatus != EXIT_SUCCESS)
                {
                    printErrorMessage(FORMAT("Could not query Perforce for the adaptive-unity working set.\n{}",
                                             result.output));
                }
                for (const string_view line : split(result.output, '\n'))
                {
                    constexpr string_view clientFile = "... clientFile ";
                    constexpr string_view movedFile = "... movedFile ";
                    if (line.starts_with(clientFile))
                    {
                        markPath(line.substr(clientFile.size()), false);
                    }
                    else if (line.starts_with(movedFile))
                    {
                        markPath(line.substr(movedFile.size()), false);
                    }
                }
            }
        }
    }

    STACK_PMR_VECTOR(Node *, nodes, 1024)
    nodes.reserve(candidates.size());
    for (const auto &[candidatePath, node] : candidates)
    {
        if (sourceControlPaths.contains(candidatePath))
        {
            workingSet.emplace(node);
        }
        if (!node->statCompleted)
        {
            nodes.emplace_back(const_cast<Node *>(node));
        }
    }
    if (nodes.empty())
    {
        return;
    }

    const uint32_t hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
    const uint32_t workerCount = std::min<uint32_t>(hardwareThreads, nodes.size());
    STACK_PMR_VECTOR(std::thread, workers, 256)
    workers.reserve(workerCount > 0 ? workerCount - 1 : 0);
    const auto statStride = [&](const uint32_t worker) {
        for (uint32_t i = worker; i < nodes.size(); i += workerCount)
        {
            nodes[i]->performSystemCheck();
        }
    };
    for (uint32_t worker = 1; worker < workerCount; ++worker)
    {
        workers.emplace_back(statStride, worker);
    }
    statStride(0);
    for (std::thread &worker : workers)
    {
        worker.join();
    }
}

void AdaptiveManager::completeRoundOne()
{
    if (roundOneCompleted)
    {
        return;
    }
    roundOneCompleted = true;

    CppTarget &owner = *target;
    const CppModType compileUnitType =
        owner.configuration->evaluate(IsCppMod::YES) ? CppModType::PRIMARY_IMPLEMENTATION : CppModType::CPP_SRC;
    const auto createCompileUnit = [&](const Node *node, const bool isAJumboBuild) -> CppSrc * {
        CppSrc *compileUnit;
        if (compileUnitType == CppModType::CPP_SRC)
        {
            compileUnit = new CppSrc(&owner, node, compileUnitType);
        }
        else
        {
            compileUnit = new CppMod(&owner, node, compileUnitType);
        }
        compileUnit->isAJumboBuild = isAJumboBuild;
        return compileUnit;
    };
    const auto getGeneratedNode = [&](const uint32_t index) {
        return Node::getHalfNode(owner.myBuildDir->filePath + slashc + std::to_string(index) + ".gen.cpp");
    };

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        for (uint32_t index = 0; index < owner.adaptiveSourceNodes.size(); ++index)
        {
            Node *sourceNode = owner.adaptiveSourceNodes[index];

            // Configure never executes round zero, so ordinary CppSrc/CppMod objects are sufficient cache owners.
            // The generated jumbo slot always needs an entry. The standalone slot may already be owned by a source
            // that was first registered normally and later moved into adaptive compilation.
            createCompileUnit(getGeneratedNode(index), true);
            const uint64_t sourceCacheName = static_cast<uint64_t>(sourceNode->myId) << 32 |
                                             static_cast<uint64_t>(owner.cacheIndex) << 3 |
                                             static_cast<uint64_t>(compileUnitType);
            const auto sourceCache = nameToIndexMap.find(sourceCacheName);
            if (sourceCache == nameToIndexMap.end() || bTargetCaches[sourceCache->second].bTarget == nullptr)
            {
                createCompileUnit(sourceNode, false);
            }
        }
        return;
    }

    if (owner.jumboFileSize == 0)
    {
        printErrorMessage(FORMAT("Adaptive-unity jumbo size must be greater than zero.\nTarget: {}", owner.name));
    }

    const auto scheduleCompileUnit = [&](CppSrc *compileUnit) {
        // File-level prerequisites are attached to this manager through getCppSrc()/getCppModule(). The selected
        // compile unit waits on the manager, and the owning CppTarget waits on the selected compile unit.
        compileUnit->realBTargets[0].addDep<BTargetType::UNKNOWN>(&realBTargets[0]);
        if (compileUnitType == CppModType::CPP_SRC)
        {
            owner.srcFileDeps.emplace_back(compileUnit);
            owner.realBTargets[0].addDep<BTargetType::CPP_SRC>(&compileUnit->realBTargets[0]);
        }
        else
        {
            owner.modFileDeps.emplace_back(static_cast<CppMod *>(compileUnit));
            owner.realBTargets[0].addDep<BTargetType::CPP_MOD>(&compileUnit->realBTargets[0]);
        }
    };

    const auto writeGeneratedPartition = [&](const uint32_t firstSourceIndex, const string &contents) {
        const Node *generatedNode = getGeneratedNode(firstSourceIndex);
        if (const string &generatedPath = generatedNode->filePath;
            !std::filesystem::exists(generatedPath) || fileToString(generatedPath) != contents)
        {
            create_directories(path(generatedPath).parent_path());
            std::ofstream(generatedPath, std::ios::binary) << contents;
        }
        scheduleCompileUnit(createCompileUnit(generatedNode, true));
    };

    constexpr string_view generatedPrefix = "// Generated by HMake adaptive unity.\n";
    uint64_t partitionSize = 0;
    uint32_t firstSourceIndex = 0;
    string generatedContents(generatedPrefix);

    const auto finishPartition = [&](const bool forceBoundary) {
        const bool hasGeneratedSources = generatedContents.size() != generatedPrefix.size();

        // UBT keeps a partition containing only working-set (virtual) files open. Its size and first slot therefore
        // carry into the next real source. An explicit HMake group boundary always resets the partition.
        if (!forceBoundary && !hasGeneratedSources)
        {
            return;
        }
        if (hasGeneratedSources)
        {
            writeGeneratedPartition(firstSourceIndex, generatedContents);
        }
        partitionSize = 0;
        generatedContents.resize(generatedPrefix.size());
    };

    uint32_t groupIndex = 0;
    for (uint32_t index = 0; index < owner.adaptiveSourceNodes.size(); ++index)
    {
        if (groupIndex < owner.adaptiveGroupStarts.size() && index == owner.adaptiveGroupStarts[groupIndex])
        {
            finishPartition(true);
            ++groupIndex;
        }
        Node *sourceNode = owner.adaptiveSourceNodes[index];
        const uint64_t sourceSize = std::max<uint64_t>(sourceNode->fileSize, 1);

        // UBT isolates an oversized source from a preceding real-file blob. A virtual-only collection deliberately
        // remains attached, as in UnityFileBuilder::EndCurrentUnityFile().
        if (sourceSize > owner.jumboFileSize)
        {
            finishPartition(false);
        }
        if (partitionSize == 0)
        {
            // The slot follows the virtual partition, even if its first source is in the working set. This keeps the
            // generated cache key stable while files enter and leave standalone adaptive compilation.
            firstSourceIndex = index;
        }

        if (workingSet.contains(sourceNode))
        {
            scheduleCompileUnit(createCompileUnit(sourceNode, false));
        }
        else
        {
            generatedContents += "#include \"";
            generatedContents += owner.getAdaptiveIncludeName(sourceNode);
            generatedContents += "\"\n";
        }

        partitionSize += sourceSize;
        // Add first and split only after exceeding the threshold; equality stays in the current collection.
        if (partitionSize > owner.jumboFileSize)
        {
            finishPartition(false);
        }
    }
    finishPartition(true);
}

string AdaptiveManager::getPrintName() const
{
    return "Adaptive unity " + target->getPrintName();
}
