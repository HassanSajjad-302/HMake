#include "Configure.hpp"
#include <cstdint>
#include <cstdlib>

#ifdef _WIN32
#include <Windows.h>
#endif

using std::filesystem::current_path;

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

    lowerCaseOnWindows(configurePathString.data(), configurePathString.size());
    configureNode = Node::getNode(configurePathString, false);

    if constexpr (bsMode == BSMode::BUILD)
    {
        for (int i = 1; i < argc; ++i)
        {
            string targetArgFullPath = (current_path() / argv[i]).lexically_normal().string();
            lowerCaseOnWindows(targetArgFullPath.data(), targetArgFullPath.size());
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

#ifdef _WIN32
BOOL WINAPI ConsoleHandler(DWORD signal)
{
    switch (signal)
    {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT: {
        vector<char> buffer;
        writeBuildBuffer(buffer);
    }
    }
    return FALSE;
}
#endif

int main2(const int argc, char **argv)
{
#ifdef _WIN32
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE))
    {
        printf("Error setting control handler\n");
        return 1;
    }
#endif
    constructGlobals();
    parseCmdArgumentsAndSetConfigureNode(argc, argv);
    initializeCache();
    (*buildSpecificationFuncPtr)();
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
