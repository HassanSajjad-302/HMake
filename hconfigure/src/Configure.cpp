#include "Configure.hpp"
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>

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

    const path currentDirectory = current_path();
    string configurePathString;
    if constexpr (bsMode != BSMode::CONFIGURE)
    {
        const path configurePath = findProjectBuildDirectory(currentDirectory);
        if (configurePath.empty())
        {
            printErrorMessage(FORMAT("Could not find cache.txt in the current directory or any parent directory.\n"
                                     "Current directory: {}\n"
                                     "Hint: run hbuild from the project's build directory first.",
                                     currentDirectory.string()));
        }
        configurePathString = configurePath.string();
    }
    else
    {
        configurePathString = currentDirectory.string();
    }

    lowerCaseOnWindows(configurePathString.data(), configurePathString.size());
    loadNodesCache(path(configurePathString) / string(nodesCacheFileName));

    if constexpr (bsMode != BSMode::BUILD)
    {
        return;
    }

    bool positionalOnly = false;
    for (int i = 1; i < argc; ++i)
    {
        const string_view argument{argv[i]};
        if (!positionalOnly)
        {
            if (argument == "--")
            {
                positionalOnly = true;
                continue;
            }
            if (argument == "--dry-run")
            {
                dryRun = true;
                continue;
            }
            if (argument == "--header-units-only")
            {
                huOnly = true;
                continue;
            }
            if (argument == "--standalone")
            {
                standAlone = true;
                continue;
            }
            if (argument == "--print-hash-map")
            {
                printHashMap = true;
                continue;
            }
            if (argument == "--jobs")
            {
                if (++i == argc)
                {
                    printErrorMessage("Missing value for generated-build option --jobs.");
                }

                const string_view value{argv[i]};
                uint16_t jobs = 0;
                const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), jobs);
                if (error != std::errc{} || end != value.data() + value.size() || jobs == 0)
                {
                    printErrorMessage(FORMAT("Invalid generated-build job count.\nValue: {}\nExpected: 1..{}", value,
                                             std::numeric_limits<uint16_t>::max()));
                }
                buildJobsOverride = jobs;
                continue;
            }
            if (argument.starts_with('-'))
            {
                printErrorMessage(FORMAT("Unknown generated-build option.\nOption: {}", argument));
            }
        }

        string targetArgFullPath = (currentDirectory / path(argument)).lexically_normal().string();
        lowerCaseOnWindows(targetArgFullPath.data(), targetArgFullPath.size());
        if (targetArgFullPath.ends_with(slashc))
        {
            targetArgFullPath.pop_back();
        }
        const auto &base = configureNode->filePath;
        if (!isPathInDirectory(targetArgFullPath, base))
        {
            printErrorMessage(FORMAT("Build target resolves outside the configured project.\n"
                                     "Argument: {}\n"
                                     "Resolved path: {}\n"
                                     "Configure directory: {}",
                                     argument, targetArgFullPath, base));
        }
        cmdTargets.emplace(targetArgFullPath.begin() + base.size() + 1, targetArgFullPath.end());
    }
}

void callConfigurationSpecification()
{
    // Specifications may append producer configurations. Only configurations present on entry have
    // configurationSpecification() invoked here; their owners initialize and finalize dynamically created
    // companions explicitly.
    const uint64_t configurationCount = allConfigurations.size();
    for (uint64_t index = 0; index < configurationCount; ++index)
    {
        Configuration &config = *allConfigurations[index];
        bool configure = config.evaluate(AlwaysConfigureThis::YES) || configureNode == currentNode;
        if (!configure)
        {
            string targetDirectory = configureNode->filePath;
            if (!targetDirectory.ends_with(slashc))
            {
                targetDirectory += slashc;
            }
            targetDirectory += config.name;
            configure = compareStringsFromEnd(currentNode->filePath, targetDirectory) ||
                        isPathInDirectory(currentNode->filePath, targetDirectory);
        }
        if (configure)
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
    if constexpr (bsMode == BSMode::BUILD)
    {
        // A per-invocation -j value has higher precedence than defaults assigned by cache.txt or buildSpecification().
        if (buildJobsOverride != 0)
        {
            projectCache.defaultJobs = buildJobsOverride;
        }
    }
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
