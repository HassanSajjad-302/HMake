
#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "Cache.hpp";
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
#include <filesystem>
#include <fstream>
#endif

using fmt::print, std::filesystem::current_path, std::filesystem::directory_iterator, std::ifstream, std::ofstream;

void writeBuildCache()
{
    std::lock_guard<std::mutex> lk(buildCacheMutex);
    prettyWritePValueToFile(pstring_view(configureDir + "/build-cache.json"), buildCache);
}

void writeBuildCacheUnlocked()
{
    writePValueToFile(pstring_view(configureDir + "/build-cache.json"), buildCache);
}

void initializeCache(BSMode bsMode_)
{
    bsMode = bsMode_;
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
            configureDir = (configurePath.parent_path().*toPStr)();
        }
        else
        {
            print(stderr, "Configure could not be found in current directory and directories above\n");
            throw std::exception();
        }
    }
    else
    {
        configureDir = (current_path().*toPStr)();
    }

    cache.initializeCacheVariableFromCacheFile();
    toolsCache.initializeToolsCacheVariableFromToolsCacheFile();

    if (bsMode != BSMode::CONFIGURE)
    {
        auto initializeSettings = [](const path &settingsFilePath) {
            Json outputSettingsJson;
            ifstream(settingsFilePath) >> outputSettingsJson;
            settings = outputSettingsJson;
        };

        initializeSettings(path(configureDir) / "settings.json");
    }
}

BSMode getBuildSystemModeFromArguments(int argc, char **argv)
{
    if (argc > 1)
    {
        pstring argument(argv[1]);
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

void actuallyReadTheCache()
{
    path p = path(configureDir) / "build-cache.json";
    if (exists(p))
    {
        pstring str = p.string();
        buildCacheFileBuffer = readPValueFromFile(str, buildCache);
    }
}

void configureOrBuild()
{
    if (bsMode == BSMode::CONFIGURE)
    {
        TCT::tarjanNodes = &tarjanNodesCTargets;
        TCT::findSCCS();
        TCT::checkForCycle();
    }
    if (bsMode == BSMode::BUILD)
    {
        actuallyReadTheCache();

        // This ensures that buildCache has the capacity to to have all the new PValue. Currently, these new PValue are
        // added in readBuildCacheFile function in CppSourceTarget and LinkOrArchiveTarget. Because, the number can't be
        // more than the following collective size, we can safely take pointer to the PValue. If not taking pointer,
        // then an extra copy will need to be performed whenever cache is to be saved and having two copies would entail
        // similar memory usage. Extra size is being reserved because targets might be already present in cache or they
        // might not and the cache of other targets which aren't to built in this cycle need to be preserved.
        buildCache.Reserve(buildCache.Size() + preSortBTargets.size(), ralloc);
        Builder{};
    }
    if (bsMode == BSMode::CONFIGURE)
    {
        vector<CTarget *> &cTargetsSortedForConfigure = TCT::topologicalSort;
        for (auto it = cTargetsSortedForConfigure.rbegin(); it != cTargetsSortedForConfigure.rend(); ++it)
        {
            it.operator*()->setJson();
        }
        for (auto it = cTargetsSortedForConfigure.rbegin(); it != cTargetsSortedForConfigure.rend(); ++it)
        {
            it.operator*()->writeJsonFile();
        }
        cache.registerCacheVariables();
    }
}
