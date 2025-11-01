
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "Cache.hpp"
#include "CacheWriteManager.hpp"
#include "CppSourceTarget.hpp"
#include "PlatformSpecific.hpp"
#include "Settings.hpp"
#include "ToolsCache.hpp"
#include "fmt/color.h"
#include "fmt/format.h"
#include <filesystem>
#include <fstream>
#include <stacktrace>

using std::filesystem::current_path, std::filesystem::directory_iterator, std::ifstream, std::ofstream;

#ifdef _WIN32
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

void initializeCache(const BSMode bsMode_)
{
    cache.initializeCacheVariableFromCacheFile();
    toolsCache.initializeToolsCacheVariableFromToolsCacheFile();

    if (const auto p = path(configureNode->filePath + slashc + getFileNameJsonOrOut("nodes")); exists(p))
    {
        const string str = p.string();
        nodesCacheGlobal = readBufferFromCompressedFile(str);

        // The Node is constructed from cache. It is placed in the hash set and also in nodeIndices.
        // However, performSystemCheck is not called and is called in multithreaded fashion.

        const uint64_t bufferSize = nodesCacheGlobal.size();
        uint64_t bufferRead = 0;
        while (bufferRead != bufferSize)
        {
            uint16_t nodeFilePathSize;
            memcpy(&nodeFilePathSize, nodesCacheGlobal.data() + bufferRead, sizeof(uint16_t));
            bufferRead += sizeof(uint16_t);
            Node::addHalfNodeFromNormalizedStringSingleThreaded(
                string(nodesCacheGlobal.data() + bufferRead, nodeFilePathSize));
            bufferRead += nodeFilePathSize;
        }
        cacheWriteManager.initialize();
    }

    currentNode = Node::getNodeFromNonNormalizedPath(current_path(), false);
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

    if constexpr (bsMode == BSMode::BUILD)
    {
        auto initializeSettings = [](const path &settingsFilePath) {
            Json outputSettingsJson;
            ifstream(settingsFilePath) >> outputSettingsJson;
            settings = outputSettingsJson;
        };

        initializeSettings(path(configureNode->filePath + slashc + "settings.json"));
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
    if (printMessagePointer)
    {
        printMessagePointer(message);
    }
    else
    {
        fmt::print("{}", message);
        fflush(stdout);
    }
}

void printMessageColor(const string &message, uint32_t color)
{
    if (printMessageColorPointer)
    {
        printMessageColorPointer(message, color);
    }
    else
    {
        if (color == static_cast<uint32_t>(fmt::color::white))
        {
            bool breakpoint = true;
        }
        fmt::print(fg(static_cast<fmt::color>(color)), "{}", message);
    }
}

static mutex printErrorMessageMutex;
void printErrorMessage(const string &message)
{
    // So the exit output is not jumbled if there are multiple errors.
    printErrorMessageMutex.lock();

    if (printErrorMessagePointer)
    {
        printErrorMessagePointer(message);
    }
    else
    {
        // fmt::print(stderr, "Error Happened.\n");
        fmt::print(stderr, "{}", message);
    }

#ifndef NDEBUG
    //  fmt::print(stderr, "{}", to_string(std::stacktrace::current()));
#endif

    errorExit();
}

void printErrorMessageNoReturn(const string &message)
{
    if (printErrorMessagePointer)
    {
        printErrorMessagePointer(message);
    }
    else
    {
        fmt::print(stderr, "{}", message);
    }
}

void printErrorMessageColor(const string &message, uint32_t color)
{
    if (printErrorMessageColorPointer)
    {
        printErrorMessageColorPointer(message, color);
    }
    else
    {
        fmt::print(stderr, fg(static_cast<fmt::color>(color)), "{}", message);
    }
}

bool configureOrBuild()
{
    const Builder b{};
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        cache.registerCacheVariables();

        vector<char> buffer;
        writeConfigBuffer(buffer);
        writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("config-cache"), buffer);

        buffer.clear();
        writeBuildBuffer(buffer);
        writeBufferToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("build-cache"), buffer);
    }
    return b.errorHappenedInRoundMode;
}

void constructGlobals()
{
    std::construct_at(&cacheWriteManager);
    BTarget::laterDepsCentral.emplace_back(&BTarget::laterDepsLocal);
    threadIds.emplace_back(getThreadId());
}

void destructGlobals()
{
    std::destroy_at(&cacheWriteManager);
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
