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
            const string argument{argv[i]};
            if (argument == "-n")
            {
                dryRun = true;
                continue;
            }

            if (argument == "-hu")
            {
                huOnly = true;
                continue;
            }

            string targetArgFullPath = (current_path() / argument).lexically_normal().string();
            lowerCaseOnWindows(targetArgFullPath.data(), targetArgFullPath.size());
            if (targetArgFullPath.size() <= configureNode->filePath.size())
            {
                printErrorMessage(FORMAT("Invalid Command-Line Argument {}\n", argument));
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
        string buffer;
        writeBuildBuffer(buffer);
    }
    }
    return FALSE;
}
#else
std::atomic<bool> signal_received;

void signal_handler(int signum)
{
    // Prevent re-entry if another signal arrives
    if (signal_received)
    {
        return;
    }
    signal_received = true;

    // Save progress
    string buffer;
    writeBuildBuffer(buffer);
    exit(EXIT_SUCCESS);
}

void registerSignalHandler()
{
    struct sigaction sa;
    sigset_t block_mask;

    // Initialize the sigaction structure
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;

    // Create a mask of signals to block during handler execution
    // This blocks ALL signals while the handler is running
    sigfillset(&block_mask);
    sa.sa_mask = block_mask;

    // SA_RESTART: Restart interrupted system calls
    sa.sa_flags = SA_RESTART;

    // Register handler for SIGINT (Ctrl+C)
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction SIGINT");
        exit(1);
    }

    // Register handler for SIGTERM (kill command)
    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("sigaction SIGTERM");
        exit(1);
    }
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
#else
    registerSignalHandler();
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
