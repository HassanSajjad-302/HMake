#include "Configure.hpp"
#include "HashValues.hpp"
using std::filesystem::current_path;

bool selectiveConfigurationSpecification(void (*ptr)(Configuration &configuration))
{
    if (equivalent(path(configureNode->filePath), current_path()))
    {
        for (const Configuration &configuration : targets<Configuration>)
        {
            (*ptr)(const_cast<Configuration &>(configuration));
        }
        return true;
    }
    for (const Configuration &configuration : targets<Configuration>)
    {
        if (configuration.isHBuildInSameOrChildDirectory())
        {
            (*ptr)(const_cast<Configuration &>(configuration));
        }
    }
    return false;
}

static void parseCmdArgumentsAndSetConfigureNode(const int argc, char **argv)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (argc > 1)
        {
            printErrorMessage("Unknown cmd arguments in configure mode\n");
            for (int i = 1; i < argc; ++i)
            {
                printErrorMessage(argv[i]);
            }
        }
    }

    string configurePathString;
    if constexpr (bsMode != BSMode::CONFIGURE)
    {
        path cacheJsonPath;
        bool cacheJsonExists = false;
        for (path p = current_path(); p.root_path() != p; p = (p / "..").lexically_normal())
        {
            cacheJsonPath = p / "cache.json";
            if (exists(cacheJsonPath))
            {
                cacheJsonExists = true;
                break;
            }
        }

        if (cacheJsonExists)
        {
            configurePathString = cacheJsonPath.parent_path().string();
        }
        else
        {
            printErrorMessage("cache.json could not be found in current dir and dirs above\n");
        }
    }
    else
    {
        configurePathString = current_path().string();
    }

    lowerCasePStringOnWindows(configurePathString.data(), configurePathString.size());
    configureNode = Node::getNodeFromNormalizedString(configurePathString, false);

    if constexpr (bsMode == BSMode::BUILD)
    {
        for (int i = 1; i < argc; ++i)
        {
            string targetArgFullPath = (current_path() / argv[i]).lexically_normal().string();
            lowerCasePStringOnWindows(targetArgFullPath.data(), targetArgFullPath.size());
            if (targetArgFullPath.size() <= configureNode->filePath.size())
            {
                printErrorMessage(FORMAT("Invalid Command-Line Argument {}\n", argv[i]));
            }
            if (targetArgFullPath.ends_with(slashc))
            {
                cmdTargets.emplace(targetArgFullPath.begin() + configureNode->filePath.size() + 1,
                                   targetArgFullPath.end() - 1);
            }
            else
            {
                cmdTargets.emplace(targetArgFullPath.begin() + configureNode->filePath.size() + 1,
                                   targetArgFullPath.end());
            }
        }
    }
}

void callConfigurationSpecification()
{
    for (Configuration &config : targets<Configuration>)
    {
        if (config.isHBuildInSameOrChildDirectory() || configureNode == currentNode)
        {
            config.initialize();
            (*configurationSpecificationFuncPtr)(config);
            config.postConfigurationSpecification();
        }
    }
}

int main2(const int argc, char **argv)
{
    parseCmdArgumentsAndSetConfigureNode(argc, argv);
    constructGlobals();
    initializeCache(bsMode);
    (*buildSpecificationFuncPtr)();
    singleThreadRunning = true;
    const bool errorHappened = configureOrBuild();
    destructGlobals();
    fflush(stdout);
    fflush(stderr);
#ifdef NDEBUG
    if (errorHappened)
    {
        std::_Exit(EXIT_FAILURE);
    }
    std::_Exit(EXIT_SUCCESS);
#else
    if (errorHappened)
    {
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
#endif
}
