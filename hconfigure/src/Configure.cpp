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
        if (const_cast<Configuration &>(configuration).isHBuildInSameOrChildDirectory())
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
            throw std::exception("cache.json could not be found in current directory and directories above\n");
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
                throw std::invalid_argument(FORMAT("Invalid Command-Line Argument {}\n", argv[i]));
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
    for (Configuration &configuration : targets<Configuration>)
    {
        if (configuration.isHBuildInSameOrChildDirectory() || configureNode == currentNode)
        {
            configuration.initialize();
            configurationSpecification(configuration);
        }
    }
}

int main2(const int argc, char **argv)
{
    try
    {
        parseCmdArgumentsAndSetConfigureNode(argc, argv);
        constructGlobals();
        initializeCache(bsMode);
        buildSpecification();
        const bool errorHappened = configureOrBuild();
        destructGlobals();
        if (errorHappened)
        {
            exit(EXIT_FAILURE);
        }
    }
    catch (std::exception &ec)
    {
        if (const string str(ec.what()); !str.empty())
        {
            printErrorMessage(str);
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
