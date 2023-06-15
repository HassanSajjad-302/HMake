
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
    ofstream(configureDir + "/build-cache.json") << buildCache.dump(4);
}

void writeBuildCacheUnlocked()
{
    ofstream(configureDir + "/build-cache.json") << buildCache.dump(4);
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
            configureDir = configurePath.parent_path().generic_string();
        }
        else
        {
            print(stderr, "Configure could not be found in current directory and directories above\n");
            throw std::exception();
        }
    }
    else
    {
        configureDir = current_path().generic_string();
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
        string argument(argv[1]);
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
        print("{}", message);
    }
}

void preintMessageColor(const string &message, uint32_t color)
{
    if (printMessageColorPointer)
    {
        printMessageColorPointer(message, color);
    }
    else
    {
        print(fg(static_cast<fmt::color>(color)), "{}", message);
    }
}

void printErrorMessage(const string &message)
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

void printErrorMessageColor(const string &message, uint32_t color)
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
        ifstream(p.string()) >> buildCache;
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
        Builder{};
    }

    /*    {
            Builder{3, 2, preSortBTargets};
        }

        if (bsMode == BSMode::BUILD)
        {
            Builder{1, 0, preSortBTargets};
        }
        else if (bsMode == BSMode::CONFIGURE)
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
        }*/
}
