
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

void setBoolsAndSetRunDir(int argc, char **argv)
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
            print(stderr, "{}", "Invoked with unknown CMD option");
            exit(EXIT_FAILURE);
        }
    }

    if (bsMode == BSMode::BUILD)
    {
        path configurePath;
        bool configureExists = false;
        for (path p = current_path(); p.root_path() != p; p = (p / "..").lexically_normal())
        {
            configurePath = p / getActualNameFromTargetName(TargetType::EXECUTABLE, os, "configure");
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
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        configureDir = current_path().generic_string();
    }

    cache.initializeCacheVariableFromCacheFile();
    toolsCache.initializeToolsCacheVariableFromToolsCacheFile();

    if (bsMode == BSMode::BUILD)
    {

        auto initializeSettings = [](const path &settingsFilePath) {
            Json outputSettingsJson;
            ifstream(settingsFilePath) >> outputSettingsJson;
            settings = outputSettingsJson;
        };

        initializeSettings(path(configureDir) / "settings.hmake");
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

    auto isCTargetInSelectedSubDirectory = [](const CTarget &cTarget) {
        path targetPath = cTarget.getSubDirForTarget();
        for (; targetPath.root_path() != targetPath; targetPath = (targetPath / "..").lexically_normal())
        {
            std::error_code ec;
            if (equivalent(targetPath, current_path(), ec))
            {
                return true;
            }
        }
        return false;
    };

    list<BTarget *> preSortBTargets;
    for (CTarget *cTarget : targetPointers<CTarget>)
    {
        if (BTarget *bTarget = cTarget->getBTarget(); bTarget)
        {
            preSortBTargets.emplace_back(bTarget);
            if (bsMode == BSMode::BUILD && isCTargetInSelectedSubDirectory(*cTarget))
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
