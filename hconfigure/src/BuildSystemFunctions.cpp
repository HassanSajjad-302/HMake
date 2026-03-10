
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "Cache.hpp"
#include "CppTarget.hpp"
#include "TargetCache.hpp"
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
#ifdef USE_JSON_FILE_COMPRESSION
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
        // later run in parallel.

        const uint64_t bufferSize = nodesCacheGlobal.size();
        uint64_t bufferRead = 0;
        while (bufferRead != bufferSize)
        {
            uint16_t nodeFilePathSize;
            memcpy(&nodeFilePathSize, nodesCacheGlobal.data() + bufferRead, sizeof(uint16_t));
            bufferRead += sizeof(uint16_t);
            Node::getHalfNode(string(nodesCacheGlobal.data() + bufferRead, nodeFilePathSize));
            bufferRead += nodeFilePathSize;
        }
        nodesSizeBefore = Node::idCount;
    }

    currentNode = Node::getNodeNonNormalized(current_path().string(), false);
    if (currentNode->filePath.size() < configureNode->filePath.size())
    {
        printErrorMessage(FORMAT("HMake internal error. configureNode size {} less than currentNode size{}\n",
                                 configureNode->filePath.size(), currentNode->filePath.size()));
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
            printErrorMessage(FORMAT("{} does not exist. Exiting\n", p.string().c_str()));
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
            printErrorMessage(FORMAT("{} does not exist. Exiting\n", p.string().c_str()));
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

void printErrorMessage(const string &message)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        string buffer;
        writeBuildBuffer(buffer);
    }

    std::print(stderr, "{}", message);

    errorExit();
}

void printErrorMessageNoReturn(const string &message)
{
    std::print(stderr, "{}", message);
}

bool configureOrBuild()
{
    const Builder b{};
    string buffer;
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        cache.registerCacheVariables();
        writeConfigBuffer(buffer);
        buffer.clear();
    }
    writeBuildBuffer(buffer);
    return b.errorHappenedInRoundMode;
}

void constructGlobals()
{
    // We intentionally skip zero-initializing these large arrays. This improves zero-target build time by roughly 4-5%.

    // ~1 MB per round; released after the round finishes.
    for (span<RealBTarget *> &realBTargets : BTarget::realBTargetsGlobal)
    {
        constexpr uint32_t count = 128 * 1024;
        const auto buffer = new char[sizeof(RealBTarget) * count];
        realBTargets = span(reinterpret_cast<RealBTarget **>(buffer), count);
    }
    nodeIndices = new Node *[128 * 1024];
    std::construct_at(&nodeAllFiles, 10000);

    std::construct_at(&cache);

#ifdef _WIN32
    std::construct_at(&unusedKeysIndices);
    unusedKeysIndices.reserve(32 * 1024);
    eventData = new CompletionKey[32 * 1024];
#else
    eventData = new BTarget *[32 * 1024];
#endif
}

void destructGlobals()
{
}

void errorExit()
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
        printErrorMessage("Error closing the file \n");
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
        printErrorMessage("Error flushing the file \n");
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
    fileBuffer.resize(filesize);
    const uint64_t readLength = fread(fileBuffer.data(), 1, filesize, fp);
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
    buffer.resize(filesize);
    const uint64_t readLength = fread(buffer.data(), 1, filesize, fp);
    fclose(fp);
}

string readBufferFromCompressedFile(const string &fileName)
{
#ifndef USE_JSON_FILE_COMPRESSION
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
        FileTargetCache fileCacheTarget;

        fileCacheTarget.name = readStringView(ptr, bufferRead);
        fileCacheTarget.configCache = readStringView(ptr, bufferRead);

        fileTargetCaches.emplace_back(fileCacheTarget);
        nameToIndexMap.emplace(fileCacheTarget.name, count);

        ++count;
    }
}

void readBuildCache()
{
    const uint32_t bufferSize = buildCacheGlobal.size();
    uint32_t bufferRead = 0;

    const char *ptr = buildCacheGlobal.data();
    for (FileTargetCache &fileCacheTarget : fileTargetCaches)
    {
        fileCacheTarget.buildCache = readStringView(ptr, bufferRead);
    }

    if (bufferRead != bufferSize)
    {
        HMAKE_HMAKE_INTERNAL_ERROR
    }
}

void writeNodesCacheIfNewNodesAdded()
{
    if (const uint64_t newNodesSize = Node::idCount; newNodesSize != nodesSizeBefore)
    {
        /*uint32_t newNodesStrSize = 0;
        newNodesStrSize += (newNodesSize - nodesSizeBefore) * 2;
        for (uint64_t i = nodesSizeBefore; i < newNodesSize; ++i)
        {
            newNodesStrSize += nodeIndices[i]->filePath.size();
        }
        nodesCacheGlobal.resize(nodesCacheGlobal.size() + newNodesStrSize);*/
        for (uint64_t i = nodesSizeBefore; i < newNodesSize; ++i)
        {
            const string &str = nodeIndices[i]->filePath;
            uint16_t strSize = str.size();
            const auto ptr = reinterpret_cast<const char *>(&strSize);
            nodesCacheGlobal.insert(nodesCacheGlobal.end(), ptr, ptr + 2);
            nodesCacheGlobal.insert(nodesCacheGlobal.end(), str.begin(), str.end());
        }
        nodesSizeBefore = newNodesSize;
        writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("nodes"), nodesCacheGlobal);
    }
}

void writeConfigBuffer(string &buffer)
{
    for (FileTargetCache &fileCacheTarget : fileTargetCaches)
    {
        writeStringView(buffer, fileCacheTarget.name);
        writeStringView(buffer, fileCacheTarget.configCache);
    }
    writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("config-cache"), buffer);
}

static std::atomic callOnce(false);
void writeBuildBuffer(string &buffer)
{
    // Ensure this runs once in build mode: either on interruption or on normal build-system shutdown.
    if (callOnce.exchange(true, std::memory_order_seq_cst))
    {
        return;
    }

    writeNodesCacheIfNewNodesAdded();

    bool cacheUpdated = false;
    for (const FileTargetCache &fileCacheTarget : fileTargetCaches)
    {
        const uint32_t currentSize = buffer.size();
        // Reserve 4 bytes for the serialized size prefix.
        writeUint32(buffer, 0);
        if (fileCacheTarget.targetCache)
        {
            if (fileCacheTarget.targetCache->writeBuildCache(buffer))
            {
                cacheUpdated = true;
            }
        }
        else
        {
            buffer.append(fileCacheTarget.buildCache.begin(), fileCacheTarget.buildCache.end());
        }
        const uint32_t size = buffer.size() - (currentSize + 4);
        memcpy(buffer.data() + currentSize, &size, sizeof(size));
    }

    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        cacheUpdated = true;
    }

    if (cacheUpdated)
    {
        writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("build-cache"), buffer);
    }
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
            printErrorMessage(FORMAT("Failed to open file for writing. Error: {}\n", P2978::getErrorString()));
        }

        // Content to write to the file
        DWORD bytesWritten;

        // Write to the file
        if (!WriteFile(hFile, buffer, bufferSize, &bytesWritten, nullptr))
        {
            printErrorMessage(FORMAT("Failed to write to file. Error: {}\n", P2978::getErrorString()));
            CloseHandle(hFile);
        }

        if (!FlushFileBuffers(hFile))
        {
            printErrorMessage(FORMAT("Failed to flush file buffers. Error: {}\n", P2978::getErrorString()));
        }

        if (bytesWritten != bufferSize)
        {
            printErrorMessage("Failed to write the full file\n");
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
                printErrorMessage(FORMAT("Error:{}\n while writing file {}\n", P2978::getErrorString(), fileName));
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
#ifndef USE_JSON_FILE_COMPRESSION
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
        if (lhs[j] != rhs[j])
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
            ptr[i] = tolower(ptr[i]);
        }
    }
}

string getNormalizedPath(path filePath)
{
    if (filePath.is_relative())
    {
        filePath = path(srcNode->filePath) / filePath;
    }
    filePath = filePath.lexically_normal();

    if constexpr (os == OS::NT)
    {
        // TODO: avoid mutating the path buffer through const_cast.
        for (auto it = const_cast<path::value_type *>(filePath.c_str()); *it != '\0'; ++it)
        {
            *it = std::tolower(*it);
        }
    }
    return filePath.string();
}

// TODO: review this function and all callers for correctness.
bool childInParentPathNormalized(const string_view parent, const string_view child)
{
    if (child.size() < parent.size())
    {
        return false;
    }

    return compareStringsFromEnd(parent, string_view(child.data(), parent.size()));
}

string addQuotes(const string_view pstr)
{
    return "\"" + string(pstr) + "\"";
}

string addEscapedQuotes(const string &pstr)
{
    const string q = R"(\")";
    return q + pstr + q;
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