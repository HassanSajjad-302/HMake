
#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "Cache.hpp";
import <DSC.hpp>;
import "CppSourceTarget.hpp";
import "fmt/format.h";
import <filesystem>;
import <fstream>;
#else
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "Cache.hpp"
#include "CppSourceTarget.hpp"
#include "fmt/format.h"
#include <DSC.hpp>
#include <filesystem>
#include <fstream>
#endif

using fmt::print, std::filesystem::current_path, std::filesystem::directory_iterator, std::ifstream, std::ofstream;

static pstring getName(pstring name)
{
#ifdef USE_JSON_FILE_COMPRESSION
    return name + ".out";
#else
    return name + ".json";
#endif
}

void writeBuildCacheUnlocked()
{
    writePValueToCompressedFile(configureNode->filePath + slashc + getName("build-cache"), buildCache);
}

void initializeCache(const BSMode bsMode_)
{
    /*targets2<CppSourceTarget>.resize(1000);
    targets2<PrebuiltBasic>.resize(1000);
    targets2<LinkOrArchiveTarget>.resize(1000);
    targets2<PrebuiltLinkOrArchiveTarget>.resize(1000);
    targets2<DSC<CppSourceTarget>>.resize(1000);*/

    bsMode = bsMode_;
    pstring configurePathString;
    if (bsMode != BSMode::CONFIGURE)
    {
        path configurePath;
        bool configureExists = false;
        for (path p = current_path(); p.root_path() != p; p = (p / "..").lexically_normal())
        {
            configurePath = p / getActualNameFromTargetName(TargetType::LIBRARY_SHARED, os, "configure");
            if (exists(configurePath))
            {
                configureExists = true;
                break;
            }
        }

        if (configureExists)
        {
            configurePathString = (configurePath.parent_path().*toPStr)();
        }
        else
        {
            print(stderr, "Configure could not be found in current directory and directories above\n");
            throw std::exception();
        }
    }
    else
    {
        configurePathString = (current_path().*toPStr)();
    }

    lowerCasePString(configurePathString);
    configureNode = Node::getNodeFromNormalizedString(configurePathString, false);

    cache.initializeCacheVariableFromCacheFile();
    toolsCache.initializeToolsCacheVariableFromToolsCacheFile();

    nodesCache.Reserve(10000, ralloc);
    if (const path p = path(configureNode->filePath + slashc + getName("nodes")); exists(p))
    {
        const pstring str = p.string();
        nodesCacheBuffer = readPValueFromCompressedFile(str, nodesCache);

        // node is constructed from cache. It is emplaced in the hash set and also in nodeIndices.
        // However performSystemCheck is not called and is called in multi-threaded fashion.
        for (PValue &value : nodesCache.GetArray())
        {
            Node::getHalfNodeFromNormalizedString(pstring(value.GetString(), value.GetStringLength()));
        }
        nodesCacheSizeBefore = nodesCache.Size();
    }

    currentNode = Node::getNodeFromNonNormalizedPath(current_path(), false);
    if (bsMode == BSMode::BUILD)
    {
        if (const path p = path(configureNode->filePath + slashc + getName("config-cache")); exists(p))
        {
            const pstring str = p.string();
            configCacheBuffer = readPValueFromCompressedFile(str, configCache);
        }
        else
        {
            printErrorMessage("config-cache.json.lz4 does not exist. Exiting\n");
            exit(EXIT_FAILURE);
        }
    }

    if (bsMode != BSMode::CONFIGURE)
    {
        auto initializeSettings = [](const path &settingsFilePath) {
            Json outputSettingsJson;
            ifstream(settingsFilePath) >> outputSettingsJson;
            settings = outputSettingsJson;
        };

        initializeSettings(path(configureNode->filePath + slashc + "settings.json"));
    }
}

BSMode getBuildSystemModeFromArguments(const int argc, char **argv)
{
    if (argc > 1)
    {
        const pstring argument(argv[1]);
        if (argument == "--build")
        {
            bsMode = BSMode::BUILD;
        }
        else
        {
            print(stderr, "{}", "getBuildSystemModeFromArguments Invoked with unknown CMD option");
            throw std::exception();
        }
    }
    return bsMode;
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
    }
}

void preintMessageColor(const pstring &message, uint32_t color)
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

void loadBuildCache()
{
    if (const path p = path(configureNode->filePath + slashc + getName("build-cache")); exists(p))
    {
        const pstring str = p.string();
        buildCacheFileBuffer = readPValueFromCompressedFile(str, buildCache);
    }
}

void configureOrBuild()
{
    if (bsMode == BSMode::BUILD)
    {
        loadBuildCache();

        // This ensures that buildCache has the capacity to to have all the new PValue. Currently, these new PValue are
        // added in readBuildCacheFile function in CppSourceTarget and LinkOrArchiveTarget. Because, the number can't be
        // more than the following collective size, we can safely take pointer to the PValue. If not taking pointer,
        // then an extra copy will need to be performed whenever cache is to be saved and having two copies would entail
        // similar memory usage. Extra size is being reserved because targets might be already present in cache or they
        // might not and the cache of other targets which aren't to built in this cycle need to be preserved.

        buildCache.Reserve(buildCache.Size() + configCache.Size(), ralloc);
        Builder{};
    }
    if (bsMode == BSMode::CONFIGURE)
    {
        Builder{};
        cache.registerCacheVariables();
        for (const PValue *pValue : targetConfigCaches)
        {
            configCache.PushBack(PValue().CopyFrom(*pValue, ralloc), ralloc);
        }
        writePValueToCompressedFile(configureNode->filePath + slashc + getName("config-cache"), configCache);
    }
    writePValueToCompressedFile(configureNode->filePath + slashc + getName("nodes"), nodesCache);
    /*if (nodesCache.Size() != nodesCacheSizeBefore)
    {
        writePValueToCompressedFile(configureNode->filePath + slashc + getName("nodes.json"), nodesCache);
    }
    assert(nodesCache.Size() >= nodesCacheSizeBefore &&
           "nodes cache size can not be less than the originally loaded file");*/
}

pstring getLastNameAfterSlash(pstring name)
{
    if (const uint64_t i = name.find_last_of(slashc); i != pstring::npos)
    {
        return {name.begin() + i + 1, name.end()};
    }
    return name;
}