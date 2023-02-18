#include "BuildSystemFunctions.hpp"
#include "Cache.hpp"
#include "CppSourceTarget.hpp"
#include "fmt/format.h"
#include <filesystem>
#include <fstream>

using fmt::print, std::filesystem::current_path, std::filesystem::exists, std::filesystem::directory_iterator,
    std::ifstream;

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
    print("buildModeConfigure {}\n", bsMode == BSMode::CONFIGURE);

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

inline void topologicallySortAndAddPrivateLibsToPublicLibs()
{
    for (CTarget *cTarget : targetPointers<CTarget>)
    {
        cTarget->populateCTargetDependencies();
    }

    TCT::tarjanNodes = &tarjanNodesCTargets;
    TCT::findSCCS();
    TCT::checkForCycle(); // TODO

    for (CTarget *cTarget : TCT::topologicalSort)
    {
        cTarget->addPrivatePropertiesToPublicProperties();
    }
}

void configureOrBuild()
{
    vector<CTarget *> cTargetsSortedForConfigure;
    if (bsMode == BSMode::CONFIGURE)
    {
        TCT::tarjanNodes = &tarjanNodesCTargets;
        TCT::findSCCS();
        TCT::checkForCycle();
        cTargetsSortedForConfigure = std::move(TCT::topologicalSort);
        tarjanNodesCTargets.clear();
        for (CTarget *cTarget : cTargetsSortedForConfigure)
        {
            cTarget->cTarjanNode = const_cast<TCT *>(tarjanNodesCTargets.emplace(cTarget).first.operator->());
        }
    }

    // All CTargets declared in the hmake.cpp have been topologically sorted based on container-elements and later
    // on will be configured. But, before that cyclic dependencies is checked for in libraries and public properties
    // need to be propagated above.
    topologicallySortAndAddPrivateLibsToPublicLibs();
    if (bsMode == BSMode::CONFIGURE)
    {
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
        Builder{};
    }
}
