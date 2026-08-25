
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "Cache.hpp"
#include "CppTarget.hpp"
#include "ToolsCache.hpp"
#include "lz4/lib/lz4.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <print>
#include <stacktrace>
#include <thread>
#include <utility>

using std::filesystem::current_path, std::filesystem::directory_iterator, std::ifstream, std::ofstream;

#ifdef _WIN32
#include <Windows.h>
#include <io.h> // For _isatty on Windows
#else
#include <unistd.h> // For isatty on Unix-like systems
#include <wordexp.h>
#endif

void setIsConsol()
{
#ifdef _WIN32
    isConsole = _isatty(_fileno(stdout));
#else
    isConsole = isatty(fileno(stdout));
#endif
}

string getFileNameJsonOrOut(const string &name)
{
#ifdef USE_FILE_COMPRESSION
    return name + ".bin.lz4";
#else
    return name + ".bin";
#endif
}

void initializeCache()
{
    cache.initializeCacheVariableFromCacheFile();
    toolsCache.initializeToolsCacheVariableFromToolsCacheFile();

    if (const auto p = path(configureNode->filePath + slashc + getFileNameJsonOrOut("nodes")); exists(p))
    {
        const string str = p.string();
        nodesCacheGlobal = readBufferFromCompressedFile(str);

        // Nodes loaded from cache are inserted into the hash set and nodeIndices. performSystemCheck() is deferred and
        // later run in parallel. Each record is uint16 path size, path, lastWriteTime, contentHash.

        constexpr size_t metadataSize = 2 * sizeof(uint64_t);
        const size_t bufferSize = nodesCacheGlobal.size();
        size_t bufferRead = 0;
        uint32_t cachedNodeIndex = 0;
        while (bufferRead < bufferSize)
        {
            if (bufferSize - bufferRead < sizeof(uint16_t))
            {
                printErrorMessage(FORMAT("Malformed nodes cache: truncated path length.\nPath: {}", str));
            }
            uint16_t nodeFilePathSize;
            memcpy(&nodeFilePathSize, nodesCacheGlobal.data() + bufferRead, sizeof(uint16_t));
            bufferRead += sizeof(uint16_t);
            if (bufferSize - bufferRead < static_cast<size_t>(nodeFilePathSize) + metadataSize)
            {
                printErrorMessage(FORMAT("Malformed nodes cache: truncated node record.\nPath: {}", str));
            }
            Node *node = Node::getHalfNode(string_view(nodesCacheGlobal.data() + bufferRead, nodeFilePathSize));
            bufferRead += nodeFilePathSize;
            memcpy(&node->lastWriteTime, nodesCacheGlobal.data() + bufferRead, sizeof(uint64_t));
            bufferRead += sizeof(uint64_t);
            memcpy(&node->contentHash, nodesCacheGlobal.data() + bufferRead, sizeof(uint64_t));
            bufferRead += sizeof(uint64_t);
            ++cachedNodeIndex;
        }
        cachedNodesCount = cachedNodeIndex;
    }

    currentNode = Node::getHalfNode(current_path().string());
    if (currentNode->filePath.size() < configureNode->filePath.size())
    {
        printErrorMessage(
            FORMAT("Internal path invariant failed: current path is shorter than configure path.\n"
                   "Configure path: {}\nConfigure path length: {}\nCurrent path: {}\nCurrent path length: {}",
                   configureNode->filePath, configureNode->filePath.size(), currentNode->filePath,
                   currentNode->filePath.size()));
    }
    if (currentNode->filePath.size() != configureNode->filePath.size())
    {
        currentMinusConfigure = string_view(currentNode->filePath.begin() + configureNode->filePath.size() + 1,
                                            currentNode->filePath.end());
    }

    if (const path p = path(configureNode->filePath + slashc + getFileNameJsonOrOut("config-cache")); exists(p))
    {
        const string str = p.string();
        configCacheGlobal = readBufferFromCompressedFile(str);
    }
    else
    {
        if constexpr (bsMode == BSMode::BUILD)
        {
            printErrorMessage(FORMAT("Required cache file does not exist.\nPath: {}\nBuild mode: BUILD", p.string()));
            errorExit();
        }
    }

    readConfigCache();

    if (const path p = path(configureNode->filePath + slashc + getFileNameJsonOrOut("build-cache")); exists(p))
    {
        const string str = p.string();
        buildCacheGlobal = readBufferFromCompressedFile(str);
        readBuildCache();
    }
    else
    {
        if constexpr (bsMode == BSMode::BUILD)
        {
            printErrorMessage(FORMAT("Required cache file does not exist.\nPath: {}\nBuild mode: BUILD", p.string()));
            errorExit();
        }
    }
}

void printDebugMessage(const string &message)
{
#ifndef NDEBUG
    printMessage(message);
#endif
}

void printMessage(const string &message)
{
    fwrite(message.c_str(), 1, message.size(), stdout);
    fflush(stdout);
}

void printMessage(const char *message)
{
    const string str(message);
    printMessage(str);
}

void printMessage(const std::pmr::string &message)
{
    fwrite(message.c_str(), 1, message.size(), stdout);
    fflush(stdout);
}

namespace
{
void writeStandardError(const string_view message)
{
    // Callers provide the diagnostic body; this function owns its presentation so all errors remain consistent.
    string_view body = message;
    if (body.starts_with("Error: "))
    {
        body.remove_prefix(7);
    }
    else if (body.starts_with("error: "))
    {
        body.remove_prefix(7);
    }

    std::print(stderr, "error: {}", body);
    if (!body.ends_with('\n'))
    {
        std::print(stderr, "\n");
    }
    fflush(stderr);
}
} // namespace

[[noreturn]] void printErrorMessage(const string &message)
{
    writeStandardError(message);
    errorExit();
}

void printErrorMessageNoReturn(const string &message)
{
    writeStandardError(message);
}

bool configureOrBuild()
{
    builderPtr = new Builder{};
    if (dryRun)
    {
        return builderPtr->errorHappenedInRoundMode;
    }

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (!builderPtr->errorHappenedInRoundMode)
        {
            cache.registerCacheVariables();
            const string configCache = getConfigCache();
            const string buildCache = getBuildCache();
            writeNodesCache();
            writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("config-cache"),
                                        configCache);
            writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("build-cache"),
                                        buildCache);
        }
    }
    else
    {
        const string buildCache = getBuildCache();
        writeNodesCache();
        if (!buildCache.empty())
        {
            writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("build-cache"),
                                        buildCache);
        }
    }

    return builderPtr->errorHappenedInRoundMode;
}

void constructGlobals()
{
    // We intentionally skip zero-initializing these large arrays. This improves zero-target build time by roughly 4-5%.

    // ~1 MB per round; released after the round finishes.
    for (span<RealBTarget *> &realBTargets : BTarget::realBTargetsGlobal)
    {
        constexpr uint32_t count = 128 * 1024;
        realBTargets = span(new RealBTarget *[count], count);
    }
    std::construct_at(&nodeIndices);
    nodeIndices.reserve(128 * 1024);
    std::construct_at(&nodeAllFiles, 10000);

    std::construct_at(&cache);

#ifdef _WIN32
    std::construct_at(&unusedKeysIndices);
    unusedKeysIndices.reserve(completionKeyCapacity);
    eventData = new CompletionKey[completionKeyCapacity];
#else
    std::construct_at(&eventData);
    eventData.resize(32 * 1024, nullptr);
#endif
}

void destructGlobals()
{
    delete builderPtr;
    builderPtr = nullptr;

    for (span<RealBTarget *> &realBTargets : BTarget::realBTargetsGlobal)
    {
        delete[] realBTargets.data();
        realBTargets = {};
    }

#ifdef _WIN32
    delete[] eventData;
    eventData = nullptr;
    std::destroy_at(&unusedKeysIndices);
#else
    std::destroy_at(&eventData);
#endif

    std::destroy_at(&cache);
    std::destroy_at(&nodeAllFiles);
    std::destroy_at(&nodeIndices);
}

[[noreturn]] void errorExit()
{
    fflush(stdout);
    fflush(stderr);
    std::_Exit(EXIT_FAILURE);
}

string getLastNameAfterSlash(string_view name)
{
    if (const uint64_t i = name.find_last_of(slashc); i != string::npos)
    {
        return {name.begin() + i + 1, name.end()};
    }
    return string(name);
}

string_view getLastNameAfterSlashV(string_view name)
{
    if (const uint64_t i = name.find_last_of(slashc); i != string::npos)
    {
        return {name.begin() + i + 1, name.end()};
    }
    return name;
}

string getNameBeforeLastSlash(string_view name)
{
    if (const uint64_t i = name.find_last_of(slashc); i != string::npos)
    {
        return {name.begin(), name.begin() + i};
    }
    return string(name);
}

string_view getNameBeforeLastSlashV(string_view name)
{
    if (const uint64_t i = name.find_last_of(slashc); i != string::npos)
    {
        return {name.begin(), name.begin() + i};
    }
    return name;
}

string getNameBeforeLastPeriod(string_view name)
{
    if (const uint64_t i = name.find_last_of('.'); i != string::npos)
    {
        return {name.begin(), name.begin() + i};
    }
    return string(name);
}

string removeDashCppFromName(string_view name)
{
    return string(name.substr(0, name.size() - 4)); // Removing -cpp from the name
}

string_view removeDashCppFromNameSV(string_view name)
{
    return {name.data(), name.size() - 4}; // Removing -cpp from the name
}

// RapidJSON helper: platform-specific output stream wrapper.
struct RHPOStream
{
    FILE *fp = nullptr;
    RHPOStream(string_view fileName);
    ~RHPOStream();
    typedef char Ch;
    void Put(Ch c) const;
    void Flush();
};

RHPOStream::RHPOStream(const string_view fileName)
{
    fp = fopen(fileName.data(), "wb");
}

RHPOStream::~RHPOStream()
{
    int result = fclose(fp);
    if (result != 0)
    {
        printErrorMessage(FORMAT("Could not close a cache file.\nSystem error: {}", P2978::getErrorString()));
    }
}

void RHPOStream::Put(const Ch c) const
{
    fputc(c, fp);
}

void RHPOStream::Flush()
{
    if (int result = fflush(fp); result != 0)
    {
        printErrorMessage(FORMAT("Could not flush a cache file.\nSystem error: {}", P2978::getErrorString()));
    }
}

string fileToString(const string &fileName)
{
    string fileBuffer;
    FILE *fp;
#ifdef WIN32
    fopen_s(&fp, fileName.data(), "rb");
#else
    fp = fopen(fileName.c_str(), "r");
#endif
    fseek(fp, 0, SEEK_END);
    const size_t filesize = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fileBuffer.resize_and_overwrite(filesize, [&](char *buf, const size_t n) { return fread(buf, 1, n, fp); });
    fclose(fp);
    return fileBuffer;
}

void fileToString(const string &fileName, std::pmr::string &buffer)
{
    FILE *fp;
#ifdef WIN32
    fopen_s(&fp, fileName.data(), "rb");
#else
    fp = fopen(fileName.c_str(), "r");
#endif
    fseek(fp, 0, SEEK_END);
    const size_t filesize = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buffer.resize_and_overwrite(filesize, [&](char *buf, const size_t n) { return fread(buf, 1, n, fp); });
    fclose(fp);
}

namespace
{
template <typename String> void appendResponseArgument(String &responseContents, const string_view argument)
{
    if (!argument.empty() && argument.find_first_of(" \t\r\n\f\v\"\\'") == string_view::npos)
    {
        responseContents.append(argument);
        responseContents.push_back('\n');
        return;
    }

    // LLVM's GNU tokenizer removes a backslash before another backslash or quote. Doubling every literal backslash and
    // escaping every double quote therefore preserves the argv produced by wordexp(), including consecutive slashes.
    responseContents.push_back('"');
    for (const char value : argument)
    {
        if (value == '\\' || value == '"')
        {
            responseContents.push_back('\\');
        }
        responseContents.push_back(value);
    }
    responseContents.push_back('"');
    responseContents.push_back('\n');
}

void writeResponseFile(const string &fileName, const string_view contents)
{
    // A response file is a transient process-transport artifact written immediately before launch. Its timestamp is
    // not part of HMake's dependency model, so reading it first merely doubles I/O and allocates an old-content buffer.
    FILE *output = nullptr;
#ifdef _WIN32
    fopen_s(&output, fileName.c_str(), "wb");
#else
    output = fopen(fileName.c_str(), "wb");
#endif
    if (output == nullptr)
    {
        printErrorMessage(FORMAT("Could not create response file.\nResponse file: {}", fileName));
    }

    const size_t written = contents.empty() ? 0 : fwrite(contents.data(), 1, contents.size(), output);
    const int closeResult = fclose(output);
    if (written != contents.size() || closeResult != 0)
    {
        printErrorMessage(FORMAT("Could not write response file.\nResponse file: {}\nRequested bytes: {}\n"
                                 "Written bytes: {}",
                                 fileName, contents.size(), written));
    }
}

template <typename String>
void commandWithResponseFileImpl(String &command, const string &responseFile, const uint64_t threshold)
{
    if (threshold == 0 || command.size() <= threshold)
    {
        return;
    }

#ifndef _WIN32
    // RunCommand uses wordexp() before execvp(). Tokenize here in exactly the same way so moving arguments to a
    // response file does not alter shell quoting, variable expansion, or escaped preprocessor definitions. wordexp()
    // owns the resulting argv, allowing the original command buffer to become the response-file buffer.
    wordexp_t expanded{};
    const int result = wordexp(command.c_str(), &expanded, WRDE_NOCMD);
    if (result != 0 || expanded.we_wordc == 0)
    {
        if (result == 0 || result == WRDE_NOSPACE)
        {
            wordfree(&expanded);
        }
        printErrorMessage(FORMAT("Could not tokenize an oversized command for its response file.\nCommand: {}\n"
                                 "Response file: {}\nwordexp result: {}",
                                 string_view(command.data(), command.size()), responseFile, result));
    }

    command.clear();
    for (size_t index = 1; index < expanded.we_wordc; ++index)
    {
        appendResponseArgument(command, expanded.we_wordv[index]);
    }
    writeResponseFile(responseFile, string_view(command.data(), command.size()));

    // Reuse the same allocation once more for the much smaller command passed through RunCommand's wordexp(). Single
    // quoting preserves every executable/response-path byte; the four-character insertion handles a literal quote.
    command.clear();
    const auto appendWordexpLiteral = [&command](const string_view value) {
        command.push_back('\'');
        for (const char character : value)
        {
            if (character == '\'')
            {
                command.append("'\\''");
            }
            else
            {
                command.push_back(character);
            }
        }
        command.push_back('\'');
    };
    appendWordexpLiteral(expanded.we_wordv[0]);
    command.append(" @");
    appendWordexpLiteral(responseFile);
    wordfree(&expanded);
#else
    // CreateProcess receives the original Windows command line directly. Preserve its existing argument spelling in
    // the response file; only separate argv[0], respecting an ordinary quoted executable path.
    const string_view commandView(command.data(), command.size());
    const size_t begin = commandView.find_first_not_of(" \t\r\n");
    if (begin == string_view::npos)
    {
        printErrorMessage("Cannot create a response file for an empty command.");
    }

    STACK_PMR_STRING(executable, 16 * 1024)
    size_t end = begin;
    if (commandView[begin] == '"')
    {
        end = commandView.find('"', begin + 1);
        if (end == string_view::npos)
        {
            printErrorMessage(
                FORMAT("Oversized command has an unterminated executable quote.\nCommand: {}", commandView));
        }
        executable.assign(commandView.substr(begin + 1, end - begin - 1));
        ++end;
    }
    else
    {
        end = commandView.find_first_of(" \t\r\n", begin);
        if (end == string_view::npos)
        {
            end = commandView.size();
        }
        executable.assign(commandView.substr(begin, end - begin));
    }

    const size_t arguments = commandView.find_first_not_of(" \t\r\n", end);
    const string_view responseContents = arguments == string_view::npos ? string_view{} : commandView.substr(arguments);
    writeResponseFile(responseFile, responseContents);

    command.clear();
    command.push_back('"');
    command.append(executable.data(), executable.size());
    command.append("\" @\"");
    command.append(responseFile);
    command.push_back('"');
#endif
}
} // namespace

void commandWithResponseFile(std::pmr::string &command, const string &responseFile, const uint64_t threshold)
{
    commandWithResponseFileImpl(command, responseFile, threshold);
}

void commandWithResponseFile(string &command, const string &responseFile, const uint64_t threshold)
{
    commandWithResponseFileImpl(command, responseFile, threshold);
}

string readBufferFromCompressedFile(const string &fileName)
{
#ifndef USE_FILE_COMPRESSION
    return fileToString(fileName);
#else
    string compressedBuffer = fileToString(fileName);
    string fileBuffer;
    fileBuffer.resize(*reinterpret_cast<uint64_t *>(compressedBuffer.data()));

    const int decompressSize =
        LZ4_decompress_safe(&compressedBuffer[8], fileBuffer.data(), compressedBuffer.size() - 8, fileBuffer.size());

    if (decompressSize < 0)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
        errorExit();
    }

    if (fileBuffer.size() != decompressSize)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
        errorExit();
    }

    return fileBuffer;

#endif
}

string getThreadId()
{
    const auto myId = std::this_thread::get_id();
    std::stringstream ss;
    ss << myId;
    return ss.str() + '\n';
}

void readConfigCache()
{
    const uint32_t bufferSize = configCacheGlobal.size();
    uint32_t bufferRead = 0;

    uint32_t count = 0;
    const char *ptr = configCacheGlobal.data();
    while (bufferRead != bufferSize)
    {
        BTargetCache bTargetCache;

        bTargetCache.name = readUint64(ptr, bufferRead);
        bTargetCache.configCache = readStringView(ptr, bufferRead);

        bTargetCaches.emplace_back(bTargetCache);
        nameToIndexMap.emplace(bTargetCache.name, count);

        ++count;
    }

    if (bufferRead != bufferSize)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }
}

void readBuildCache()
{
    const uint32_t bufferSize = buildCacheGlobal.size();
    uint32_t bytesRead = 0;

    const char *ptr = buildCacheGlobal.data();
    for (BTargetCache &fileCacheTarget : bTargetCaches)
    {
        // reading the deps-cache-inline
        const uint32_t offset = bytesRead;
        const uint32_t depsSize = readUint32(ptr, bytesRead);
        bytesRead += 4 * depsSize;

        fileCacheTarget.depsCache = {ptr + offset, bytesRead - offset};
        fileCacheTarget.setBuildCache(readStringView(ptr, bytesRead));
    }

    if (bytesRead != bufferSize)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }
}

void writeNodesCache()
{
    const uint32_t newNodesSize = Node::idCount;
    bool nodesCacheChanged = newNodesSize != cachedNodesCount;

    size_t pos = 0;
    for (uint32_t i = 0; i < cachedNodesCount; ++i)
    {
        const Node *node = nodeIndices[i];
        uint16_t strSize;
        memcpy(&strSize, nodesCacheGlobal.data() + pos, sizeof(uint16_t));
        pos += sizeof(uint16_t) + strSize;

        if (node->hashCompleted)
        {
            uint64_t persistedLastWriteTime;
            memcpy(&persistedLastWriteTime, nodesCacheGlobal.data() + pos, sizeof(persistedLastWriteTime));
            if (node->lastWriteTime != persistedLastWriteTime)
            {
                const uint64_t metadata[] = {node->lastWriteTime, node->contentHash};
                memcpy(nodesCacheGlobal.data() + pos, metadata, sizeof(metadata));
                nodesCacheChanged = true;
            }
        }
        pos += 2 * sizeof(uint64_t);
    }

    for (uint32_t i = cachedNodesCount; i < newNodesSize; ++i)
    {
        const Node *node = nodeIndices[i];
        const uint16_t pathSize = static_cast<uint16_t>(node->filePath.size());
        nodesCacheGlobal.append(reinterpret_cast<const char *>(&pathSize), sizeof(pathSize));
        nodesCacheGlobal.append(node->filePath);
        uint64_t metadata[2];
        if (node->hashCompleted)
        {
            metadata[0] = node->lastWriteTime;
            metadata[1] = node->contentHash;
        }
        else
        {
            metadata[0] = UINT64_MAX;
            metadata[1] = 0;
        }
        nodesCacheGlobal.append(reinterpret_cast<const char *>(metadata), sizeof(metadata));
    }

    if (!nodesCacheChanged)
    {
        return;
    }
    cachedNodesCount = newNodesSize;
    writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("nodes"), nodesCacheGlobal);
}

string getConfigCache()
{
    string configCache;
    for (const BTargetCache &fileCacheTarget : bTargetCaches)
    {
        writeUint64(configCache, fileCacheTarget.name);

        const uint32_t currentSize = configCache.size();
        // Reserve 4 bytes for the serialized size prefix.
        writeUint32(configCache, 0);
        if (fileCacheTarget.bTarget)
        {
            fileCacheTarget.bTarget->writeConfigCacheAtConfigTime(configCache);
        }

        // writing size to the placeholder above.
        const uint32_t size = configCache.size() - (currentSize + 4);
        memcpy(configCache.data() + currentSize, &size, sizeof(size));
    }
    return configCache;
}

string getBuildCache()
{
    string buildCache;
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        for (const BTargetCache &fileCacheTarget : bTargetCaches)
        {
            if (fileCacheTarget.depsCache.empty())
            {
                writeUint32(buildCache, 0); // no deps yet: write count = 0
            }
            else
            {
                buildCache.append(fileCacheTarget.depsCache.data(), fileCacheTarget.depsCache.size());
            }

            const uint32_t currentSize = buildCache.size();
            // Reserve 4 bytes for the serialized size prefix.
            writeUint32(buildCache, 0);
            if (BTarget *bt = fileCacheTarget.bTarget; bt && fileCacheTarget.bTarget->newlyAdded)
            {
                fileCacheTarget.bTarget->writeBuildCacheAtConfigTime(buildCache);
                if (bt->launchesProcess)
                {
                    writeUint64(buildCache, bt->realBTargets[0].cumulativeHash);
                    writeUint64(buildCache, bt->realBTargets[0].completionTime);
                }
            }
            else
            {
                buildCache.append(fileCacheTarget.getFullBuildCache().begin(),
                                  fileCacheTarget.getFullBuildCache().end());
            }

            // writing size to the placeholder above.
            const uint32_t size = buildCache.size() - (currentSize + 4);
            memcpy(buildCache.data() + currentSize, &size, sizeof(size));

            if (ndeb == NDEB::NO)
            {
                if (BTarget *bt = fileCacheTarget.bTarget; bt && fileCacheTarget.bTarget->newlyAdded)
                {
                    fileCacheTarget.bTarget->verifyBuildCache(string_view{buildCache.data() + currentSize + 4, size});
                }
            }
        }
        return buildCache;
    }

    bool cacheUpdated = false;
    for (const BTargetCache &fileCacheTarget : bTargetCaches)
    {
        if (fileCacheTarget.bTarget)
        {
            if (fileCacheTarget.bTarget->buildCacheUpdated || fileCacheTarget.bTarget->buildFooterUpdated)
            {
                cacheUpdated = true;
                break;
            }
        }
    }
    if (!cacheUpdated)
    {
        return buildCache;
    }

    Builder::checkNodes();
    for (const BTargetCache &fileCacheTarget : bTargetCaches)
    {
        if (fileCacheTarget.depsCache.empty())
        {
            writeUint32(buildCache, 0); // no deps yet: write count = 0
        }
        else
        {
            buildCache.append(fileCacheTarget.depsCache.data(), fileCacheTarget.depsCache.size());
        }

        const uint32_t currentSize = buildCache.size();
        // Reserve 4 bytes for the serialized size prefix.
        writeUint32(buildCache, 0);

        if (const BTarget *bt = fileCacheTarget.bTarget; bt && (bt->buildFooterUpdated || bt->buildCacheUpdated))
        {
            if (fileCacheTarget.bTarget->buildCacheUpdated)
            {
                fileCacheTarget.bTarget->writeBuildCacheAtBuildTime(buildCache);
                fileCacheTarget.bTarget->writeBuildCacheFooterAtBuildTime(buildCache);
            }
            else if (fileCacheTarget.bTarget->buildFooterUpdated)
            {
                buildCache.append(fileCacheTarget.getBuildCache().begin(), fileCacheTarget.getBuildCache().end());
                fileCacheTarget.bTarget->writeBuildCacheFooterAtBuildTime(buildCache);
            }
        }
        else
        {
            buildCache.append(fileCacheTarget.getFullBuildCache().begin(), fileCacheTarget.getFullBuildCache().end());
        }

        // writing size to the placeholder above.
        const uint32_t size = buildCache.size() - (currentSize + 4);
        memcpy(buildCache.data() + currentSize, &size, sizeof(size));

        if (ndeb == NDEB::NO)
        {
            if (const BTarget *bt = fileCacheTarget.bTarget; bt && (bt->buildFooterUpdated || bt->buildCacheUpdated))
            {
                const string_view written{buildCache.data() + currentSize + 4, size};
                fileCacheTarget.bTarget->verifyBuildCache(written);
            }
        }
    }
    return buildCache;
}

#ifndef _WIN32
#define fopen_s(pFile, filename, mode) ((*(pFile)) = fopen((filename), (mode))) == NULL
#endif

// cache files are written atomically.
static void writeFileAtomically(const string &fileName, const char *buffer, uint64_t bufferSize, bool binary)
{
    const string str = fileName + ".tmp";
    if constexpr (bsMode == BSMode::BUILD)
    {
#ifdef WIN32
        // Open the existing file for writing, replacing its content
        const HANDLE hFile = CreateFile(str.c_str(),
                                        GENERIC_WRITE,         // Open for writing
                                        0,                     // Do not share
                                        NULL,                  // Default security
                                        CREATE_ALWAYS,         // Always create a new file (replace if exists)
                                        FILE_ATTRIBUTE_NORMAL, // Normal file
                                        NULL                   // No template
        );

        // Check if the file handle is valid
        if (hFile == INVALID_HANDLE_VALUE)
        {
            printErrorMessage(FORMAT("Could not open the temporary output file.\nPath: {}\nSystem error: {}", str,
                                     P2978::getErrorString()));
        }

        // Content to write to the file
        DWORD bytesWritten;

        // Write to the file
        if (!WriteFile(hFile, buffer, bufferSize, &bytesWritten, nullptr))
        {
            printErrorMessage(FORMAT("Could not write the temporary output file.\nPath: {}\nRequested bytes: {}\n"
                                     "System error: {}",
                                     str, bufferSize, P2978::getErrorString()));
            CloseHandle(hFile);
        }

        if (!FlushFileBuffers(hFile))
        {
            printErrorMessage(FORMAT("Could not flush the temporary output file.\nPath: {}\nSystem error: {}", str,
                                     P2978::getErrorString()));
        }

        if (bytesWritten != bufferSize)
        {
            printErrorMessage(
                FORMAT("Temporary output file was only partially written.\nPath: {}\nRequested bytes: {}\n"
                       "Written bytes: {}",
                       str, bufferSize, bytesWritten));
        }

        // Close the file handle
        CloseHandle(hFile);

#else
        // This code path is not used on Windows.
        if (binary)
        {
            std::ofstream f(str, std::ios::binary);
            f.write(buffer, bufferSize);
        }
        else
        {
            std::ofstream(str) << buffer;
        }
#endif
    }

    else
    {
        if (binary)
        {
            std::ofstream f(fileName, std::ios::binary);
            f.write(buffer, bufferSize);
        }
        else
        {
            std::ofstream(fileName) << buffer;
        }
    }

    if constexpr (bsMode == BSMode::BUILD)
    {
#ifdef WIN32
        // Use ReplaceFile API which is designed for atomic replacement
        if (!ReplaceFileA(fileName.c_str(), // File to be replaced
                          str.c_str(),      // Replacement file
                          NULL,             // No backup
                          0,                // No flags
                          NULL,             // Reserved
                          NULL))            // Reserved
        {
            // If ReplaceFile fails (e.g., target doesn't exist), fall back to MoveFileEx
            if (!MoveFileExA(str.c_str(), fileName.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                printErrorMessage(FORMAT("Could not replace the destination file atomically.\nTemporary path: {}\n"
                                         "Destination path: {}\nSystem error: {}",
                                         str, fileName, P2978::getErrorString()));
            }
        }
#else

        if (rename(str.c_str(), fileName.c_str()) != 0)
        {
            printMessage(
                FORMAT("Renaming File from {} to {} Not Successful. Error {}\n", str.c_str(), fileName.c_str(), errno));
        }

#endif
    }
}

void writeBufferToCompressedFile(const string &fileName, const string &fileBuffer)
{
#ifndef USE_FILE_COMPRESSION
    writeFileAtomically(fileName, fileBuffer.data(), fileBuffer.size(), true);
#else
    const uint64_t maxCompressedSize = LZ4_compressBound(fileBuffer.size());

    string compressed;
    compressed.resize(maxCompressedSize + 8);

    const int compressedSize =
        LZ4_compress_default(fileBuffer.data(), compressed.data() + 8, fileBuffer.size(), maxCompressedSize);

    // printMessage(FORMAT("\n{}\n{}\n", buffer.GetLength(), compressedSize + 8));
    if (!compressedSize)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
        errorExit();
    }
    const uint64_t fileSize = fileBuffer.size();
    memcpy(compressed.data(), &fileSize, sizeof(fileSize));

    writeFileAtomically(fileName, compressed.c_str(), compressedSize + 8, true);
#endif
}

bool compareStringsFromEnd(const string_view lhs, const string_view rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    for (int64_t j = lhs.size() - 1; j >= 0; --j)
    {
        if (lhs[index] != rhs[index])
        {
            return false;
        }
    }
    return true;
}

void lowerCaseOnWindows(char *ptr, const uint64_t size)
{
    if constexpr (os == OS::NT)
    {
        for (uint64_t i = 0; i < size; ++i)
        {
            ptr[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(ptr[i])));
        }
    }
}

string getNormalizedPath(path filePath)
{
    if (filePath.is_relative())
    {
        filePath = path(normalizationBasePath) / filePath;
    }
    filePath = filePath.lexically_normal();
    string result = filePath.string();
    lowerCaseOnWindows(result.data(), result.size());
    return result;
}

bool isPathInConfigureDirectory(const string_view filePath)
{
    const string_view configurePath = configureNode->filePath;
    const size_t configurePathSize = configurePath.size();

    // Reject almost every source-tree header with two O(1) checks before comparing the path prefix.
    if (filePath.size() <= configurePathSize || filePath[configurePathSize] != slashc)
    {
        return false;
    }
    return compareStringsFromEnd(configurePath, {filePath.data(), configurePathSize});
}

string addQuotes(const string_view pstr)
{
    string result;
    result.resize_and_overwrite(pstr.size() + 2, [&](char *buf, size_t) noexcept {
        buf[0] = '\"';
        memcpy(buf + 1, pstr.data(), pstr.size());
        buf[pstr.size() + 1] = '\"';
        return pstr.size() + 2;
    });
    return result;
}

vector<string_view> split(string_view str, const char token)
{
    vector<string_view> result;
    size_t start = 0;
    size_t end = str.find(token);

    while (end != string::npos)
    {
        result.emplace_back(str.data() + start, end - start);
        start = end + 1;
        end = str.find(token, start);
    }
    // Add the last segment (or the entire string if no token was found)
    result.emplace_back(str.data() + start, str.length() - start);

    return result;
}
std::string toString(uint32_t value)
{
    char buffer[8];

    for (int i = 7; i >= 0; --i)
    {
        constexpr char hexChars[] = "0123456789ABCDEF";
        buffer[i] = hexChars[value & 0xF];
        value >>= 4;
    }

    return {buffer, 8};
}
