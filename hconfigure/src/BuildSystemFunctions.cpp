
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

using fmt::print, std::filesystem::current_path, std::filesystem::exists, std::filesystem::directory_iterator,
    std::ifstream, std::list;

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

        initializeSettings(path(configureDir) / "settings.hmake");
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
        print(fg(static_cast<fmt::color>(color)), message);
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
        print(stderr, fg(static_cast<fmt::color>(color)), message);
    }
}

void configureOrBuild()
{
    if (bsMode == BSMode::IDE)
    {
        return;
    }
    if (bsMode == BSMode::CONFIGURE)
    {
        TCT::tarjanNodes = &tarjanNodesCTargets;
        TCT::findSCCS();
        TCT::checkForCycle();
    }

    list<BTarget *> preSortBTargets;
    for (CTarget *cTarget : targetPointers<CTarget>)
    {
        if (BTarget *bTarget = cTarget->getBTarget(); bTarget)
        {
            preSortBTargets.emplace_back(bTarget);
            if (cTarget->isCTargetInSelectedSubDirectory())
            {
                bTarget->selectiveBuild = true;
            }
        }
    }
    {
        Builder{3, 2, preSortBTargets};
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
    else
    {
        Builder{1, 0, preSortBTargets};
    }
}
