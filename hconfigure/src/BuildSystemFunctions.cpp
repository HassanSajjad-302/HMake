
#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "Cache.hpp";
import <DSC.hpp>;
import "CppSourceTarget.hpp";
import "TargetCacheDiskWriteManager.hpp";
import "fmt/format.h";
import <filesystem>;
import <fstream>;
#else
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "Cache.hpp"
#include "CppSourceTarget.hpp"
#include "TargetCacheDiskWriteManager.hpp"
#include "fmt/format.h"
#include <DSC.hpp>
#include <filesystem>
#include <fstream>
#endif

using fmt::print, std::filesystem::current_path, std::filesystem::directory_iterator, std::ifstream, std::ofstream;

pstring getFileNameJsonOrOut(const pstring &name)
{
#ifdef USE_JSON_FILE_COMPRESSION
    return name + ".out";
#else
    return name + ".json";
#endif
}

void initializeCache(const BSMode bsMode_)
{
    cache.initializeCacheVariableFromCacheFile();
    toolsCache.initializeToolsCacheVariableFromToolsCacheFile();

    if (const path p = path(configureNode->filePath + slashc + getFileNameJsonOrOut("nodes")); exists(p))
    {
        const pstring str = p.string();
        nodesCacheBuffer = readPValueFromCompressedFile(str, nodesCacheJson);

        // node is constructed from cache. It is emplaced in the hash set and also in nodeIndices.
        // However performSystemCheck is not called and is called in multi-threaded fashion.
        for (PValue &value : nodesCacheJson.GetArray())
        {
            Node::addHalfNodeFromNormalizedStringSingleThreaded(pstring(value.GetString(), value.GetStringLength()));
        }
        targetCacheDiskWriteManager.initialize();
    }

    currentNode = Node::getNodeFromNonNormalizedPath(current_path(), false);
    if (currentNode->filePath.size() < configureNode->filePath.size())
    {
        throw std::exception("HMake internal error. configureNode size less than currentNode\n");
    }
    if (currentNode->filePath.size() != configureNode->filePath.size())
    {
        currentMinusConfigure = pstring_view(currentNode->filePath.begin() + configureNode->filePath.size() + 1,
                                             currentNode->filePath.end());
    }

    if (const path p = path(configureNode->filePath + slashc + getFileNameJsonOrOut("config-cache")); exists(p))
    {
        const pstring str = p.string();
        configCacheBuffer = readPValueFromCompressedFile(str, configCache);
    }
    else
    {
        if constexpr (bsMode == BSMode::BUILD)
        {
            printErrorMessage(fmt::format("{} does not exist. Exiting\n", p.string().c_str()));
            exit(EXIT_FAILURE);
        }
    }

    if (const path p = path(configureNode->filePath + slashc + getFileNameJsonOrOut("build-cache")); exists(p))
    {
        const pstring str = p.string();
        buildCacheBuffer = readPValueFromCompressedFile(str, buildCache);
    }
    else
    {
        if constexpr (bsMode == BSMode::BUILD)
        {
            printErrorMessage(fmt::format("{} does not exist. Exiting\n", p.string().c_str()));
            exit(EXIT_FAILURE);
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

void printDebugMessage(const pstring &message)
{
#ifndef NDEBUG
    printMessage(message);
#endif
}

void printMessage(const pstring &message)
{
    if (printMessagePointer)
    {
        printMessagePointer(message);
    }
    else
    {
        print("{}", message);
        fflush(stdout);
    }
}

void printMessageColor(const pstring &message, uint32_t color)
{
    if (printMessageColorPointer)
    {
        printMessageColorPointer(message, color);
    }
    else
    {
        if (color == (uint32_t)fmt::color::white)
        {
            bool breakpoint = true;
        }
        print(fg(static_cast<fmt::color>(color)), "{}", message);
    }
}

void printErrorMessage(const pstring &message)
{
    if (printErrorMessagePointer)
    {
        printErrorMessagePointer(message);
    }
    else
    {
        print(stderr, "{}", message);
    }
}

void printErrorMessageColor(const pstring &message, uint32_t color)
{
    if (printErrorMessageColorPointer)
    {
        printErrorMessageColorPointer(message, color);
    }
    else
    {
        print(stderr, fg(static_cast<fmt::color>(color)), "{}", message);
    }
}

bool configureOrBuild()
{
    const Builder b{};
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        cache.registerCacheVariables();
        writePValueToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("config-cache"),
                                    configCache);
        writePValueToCompressedFile(configureNode->filePath + slashc + getFileNameJsonOrOut("build-cache"), buildCache);
    }
    return b.errorHappenedInRoundMode;
}

void constructGlobals()
{
    std::construct_at(&targetCacheDiskWriteManager);
}

void destructGlobals()
{
    std::destroy_at(&targetCacheDiskWriteManager);
}

pstring getLastNameAfterSlash(pstring_view name)
{
    if (const uint64_t i = name.find_last_of(slashc); i != pstring::npos)
    {
        return {name.begin() + i + 1, name.end()};
    }
    return pstring(name);
}

pstring_view getLastNameAfterSlashView(pstring_view name)
{
    if (const uint64_t i = name.find_last_of(slashc); i != pstring::npos)
    {
        return {name.begin() + i + 1, name.end()};
    }
    return name;
}
inline pstring getNameBeforeLastSlash(pstring_view name)
{
    if (const uint64_t i = name.find_last_of(slashc); i != pstring::npos)
    {
        return {name.begin(), name.begin() + i};
    }
    return pstring(name);
}

pstring removeDashCppFromName(pstring_view name)
{
    return pstring(name.substr(0, name.size() - 4)); // Removing -cpp from the name
}
