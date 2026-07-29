#include "Configure.hpp"
#include <cstdint>
#include <cstdlib>
#include <fstream>

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
            string arguments;
            for (int i = 1; i < argc; ++i)
            {
                arguments += FORMAT("\n  - {}", argv[i]);
            }
            printErrorMessage(
                FORMAT("Configure mode does not accept command-line arguments.\nArguments:{}", arguments));
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
            printErrorMessage(FORMAT("Could not find cache.json in the current directory or any parent directory.\n"
                                     "Current directory: {}\n"
                                     "Hint: run hhelper from the project's build directory first.",
                                     current_path().string()));
        }
    }
    else
    {
        configurePathString = current_path().string();
    }

    lowerCaseOnWindows(configurePathString.data(), configurePathString.size());
    configureNode = Node::getHalfNode(configurePathString);

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
            if (argument == "-s")
            {
                standAlone = true;
            }
            if (argument == "-p")
            {
                printHashMap = true;
            }

            string targetArgFullPath = (current_path() / argument).lexically_normal().string();
            lowerCaseOnWindows(targetArgFullPath.data(), targetArgFullPath.size());
            if (targetArgFullPath.size() <= configureNode->filePath.size())
            {
                printErrorMessage(FORMAT("Build target resolves outside the configured project.\n"
                                         "Argument: {}\n"
                                         "Resolved path: {}\n"
                                         "Configure directory: {}",
                                         argument, targetArgFullPath, configureNode->filePath));
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
    for (Configuration *configPointer : allConfigurations)
    {
        Configuration &config = *configPointer;
        if (config.isHBuildInSameOrChildDirectory() || configureNode == currentNode)
        {
            config.initialize();
            (*configurationSpecificationFuncPtr)(config);
            config.postConfigurationSpecification();
        }
    }
}

void printHashMapFile()
{
    string buffer;
    for (Configuration *configPointer : allConfigurations)
    {
        Configuration &config = *configPointer;
        for (CppTarget *t : config.cppTargets)
        {
            if (!t->useIPC)
            {
                continue;
            }
            buffer += "CppTarget " + t->name + '\n';
            for (const auto &[req, hfOrCppMod] : t->reqHeaderNameMapping)
            {
                if (hfOrCppMod.type == FileType::HEADER_UNIT)
                {
                    buffer += "C++20-Header-Unit \n";
                }
                else if (hfOrCppMod.type == FileType::MODULE)
                {
                    buffer += "C++20-Module \n";
                }
                {
                    buffer += "Header-File \n";
                }
                buffer += req;
                buffer += '\n';
                if (hfOrCppMod.type == FileType::HEADER_FILE)
                {
                    buffer += hfOrCppMod.data.node->filePath;
                }
                else
                {
                    buffer += hfOrCppMod.data.cppMod->node->filePath;
                }
                buffer += '\n';
            }
        }

        for (const auto &[req, vec] : config.headerNameMapping)
        {
            for (const HfOrCppMod hfOrCppMod : vec)
            {
                if (hfOrCppMod.type == FileType::HEADER_UNIT)
                {
                    buffer += "C++20-Header-Unit \n";
                }
                else if (hfOrCppMod.type == FileType::MODULE)
                {
                    buffer += "C++20-Module \n";
                }
                {
                    buffer += "Header-File \n";
                }
                buffer += req;
                buffer += '\n';
                if (hfOrCppMod.type == FileType::HEADER_FILE)
                {
                    buffer += hfOrCppMod.data.node->filePath;
                }
                else
                {
                    buffer += hfOrCppMod.data.cppMod->node->filePath;
                }
                buffer += '\n';
            }
        }
    }
    std::ofstream(configureNode->filePath + slashc + string("hash-map.txt")) << buffer;
}

int main2(const int argc, char **argv)
{
    constructGlobals();
    parseCmdArgumentsAndSetConfigureNode(argc, argv);
    initializeCache();
    (*buildSpecificationFuncPtr)();
    bool errorHappened = false;
    if constexpr (bsMode == BSMode::BUILD)
    {
        if (printHashMap)
        {
            printHashMapFile();
        }
        else
        {
            errorHappened = configureOrBuild();
        }
    }
    else
    {
        errorHappened = configureOrBuild();
    }
    destructGlobals();
    fflush(stdout);
    fflush(stderr);

    // TODO: verify there are no leaked file handles or file descriptors. builder->serverFd is not closed because
    // closing it might cause an error as there could be an unhandled interrupt event. but it is alright since we have
    // already written the cache. or a better solution could be to return from the builder constructor instead of
    // returning from here.
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
