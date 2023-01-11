#include "BuildSystemFunctions.hpp"
#include "Cache.hpp"
#include "Target.hpp"
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
        cache.saveCache = true;
    }

    cache.initializeCacheVariableFromCacheFile();

    if (bsMode == BSMode::BUILD)
    {

        auto initializeSettings = [](const path &settingsFilePath) {
            Json outputSettingsJson;
            ifstream(settingsFilePath) >> outputSettingsJson;
            settings = outputSettingsJson;
        };

        initializeSettings(path(configureDir) / "settings.hmake");

        // If current-directory has a target, it is added, else its subdirectories are checked for targets.
        if (exists("target.json"))
        {
            buildTargetFilePaths.emplace((current_path() / "target.json").string());
        }
        else
        {
            for (const auto &dir : directory_iterator(current_path()))
            {
                std::error_code ec;
                if (dir.is_directory(ec))
                {
                    path p = dir.path() / "target.json";
                    if (exists(p))
                    {
                        buildTargetFilePaths.emplace(p.generic_string());
                    }
                }
                if (ec)
                {
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
}

inline void topologicallySortAndAddPrivateLibsToPublicLibs()
{
    for (auto &[first, cTarget] : cTargets)
    {
        // TODO
        //  Prebuilt Targets might have circular dependencies as well. But, that is not checked.
        if (cTarget->cTargetType == TargetType::LIBRARY_STATIC || cTarget->cTargetType == TargetType::LIBRARY_SHARED ||
            cTarget->cTargetType == TargetType::EXECUTABLE)
        {
            auto *target = static_cast<Target *>(cTarget);
            target->addCTargetDependency(target->publicLibs);
            target->addCTargetDependency(target->privateLibs);
        }
    }

    TCT::tarjanNodes = &tarjanNodesCTargets;
    TCT::findSCCS();
    TCT::checkForCycle([](CTarget *target) -> string { return "cycle check function not implemented"; }); // TODO

    for (CTarget *cTarget : TCT::topologicalSort)
    {
        auto *target = static_cast<Target *>(cTarget);
        target->addPrivatePropertiesToPublicProperties();
    }
}

void configureOrBuild()
{
    vector<CTarget *> cTargetsSortedForConfigure;
    if (bsMode == BSMode::CONFIGURE)
    {
        TCT::tarjanNodes = &tarjanNodesCTargets;
        TCT::findSCCS();
        TCT::checkForCycle([](CTarget *target) -> string { return "cycle check function not implemented"; });
        cTargetsSortedForConfigure = std::move(TCT::topologicalSort);
        tarjanNodesCTargets.clear();
        for (CTarget *cTarget : cTargetsSortedForConfigure)
        {
            // Following pointed to the element of tarjanNodesCTargets which is cleared above
            cTarget->cTarjanNode = nullptr;
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
            it.operator*()->configure();
        }
    }
    else
    {
        Builder{};
    }
}
