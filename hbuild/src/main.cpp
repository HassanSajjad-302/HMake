#include "BuildTools.hpp"
#include "Builder.hpp"
#include "Node.hpp"
#include "ParseHeaderDeps.hpp"
#include "ProjectCache.hpp"
#include "RunCommand.hpp"
#include "Toolchains.hpp"

#include <cassert>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace
{
using std::string;
using std::string_view;
using std::vector;
using std::filesystem::path;

struct Options
{
    string_view buildDirectory;
    string_view toolchain;
    string_view calibrationCompiler;
    string_view calibrationName;
    string_view removalName;
    uint16_t defaultJobs = 0;
    vector<string_view> buildArguments;
    bool configureOnly = false;
    bool reconfigure = false;
    bool recompile = false;
    bool listToolchains = false;
    bool project = false;
    bool help = false;
};

Options parseOptions(const int argc, char **argv)
{
    Options options;
    const auto takeValue = [&](int &index, const string_view option) {
        if (index + 1 >= argc)
        {
            printErrorMessage("Option requires a value: " + string(option));
        }
        const string_view value = argv[++index];
        if (value.empty())
        {
            printErrorMessage("Option value cannot be empty: " + string(option));
        }
        return value;
    };
    STACK_PMR_VECTOR(string_view, targets, 16)
    for (int index = 1; index < argc; ++index)
    {
        const string_view argument(argv[index]);
        if (!argument.starts_with('-'))
        {
            targets.emplace_back(argument);
        }
        else if (argument == "--")
        {
            targets.insert(targets.end(), argv + index + 1, argv + argc);
            break;
        }
        else if (argument == "--help")
        {
            options.help = true;
        }
        else if (argument == "--list-toolchains")
        {
            options.listToolchains = true;
        }
        else if (argument == "-B")
        {
            options.buildDirectory = takeValue(index, argument);
        }
        else if (argument == "--toolchain")
        {
            options.toolchain = takeValue(index, argument);
        }
        else if (argument == "--calibrate")
        {
            if (!options.calibrationCompiler.empty())
            {
                printErrorMessage("Option may only be specified once: --calibrate");
            }
            options.calibrationCompiler = takeValue(index, argument);
        }
        else if (argument == "--name")
        {
            if (!options.calibrationName.empty())
            {
                printErrorMessage("Option may only be specified once: --name");
            }
            options.calibrationName = takeValue(index, argument);
        }
        else if (argument == "--remove-toolchain")
        {
            if (!options.removalName.empty())
            {
                printErrorMessage("Option may only be specified once: --remove-toolchain");
            }
            options.removalName = takeValue(index, argument);
        }
        else if (argument == "--project")
        {
            options.project = true;
        }
        else if (argument == "--default-jobs" || argument == "-j")
        {
            const string_view value = takeValue(index, argument);
            uint16_t jobs = 0;
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), jobs);
            if (error != std::errc{} || end != value.data() + value.size() || jobs == 0)
            {
                printErrorMessage("Expected an integer from 1 through 65535 after " + string(argument) + ": " +
                                  string(value));
            }
            if (argument == "--default-jobs")
            {
                options.defaultJobs = jobs;
            }
            else
            {
                options.buildArguments.emplace_back("--jobs");
                options.buildArguments.emplace_back(value);
            }
        }
        else if (argument == "--configure-only")
        {
            options.configureOnly = true;
        }
        else if (argument == "--reconfigure")
        {
            options.reconfigure = true;
        }
        else if (argument == "--recompile")
        {
            options.recompile = true;
            options.reconfigure = true;
        }
        else if (argument == "--dry-run" || argument == "--header-units-only" || argument == "--standalone" ||
                 argument == "--print-hash-map")
        {
            options.buildArguments.emplace_back(argument);
        }
        else
        {
            printErrorMessage("Unknown hbuild option: " + string(argument));
        }
    }

    if (!targets.empty())
    {
        options.buildArguments.emplace_back("--");
        options.buildArguments.insert(options.buildArguments.end(), targets.begin(), targets.end());
    }
    if (!options.calibrationName.empty() && options.calibrationCompiler.empty())
    {
        printErrorMessage("--name requires --calibrate <compiler-path-or-name>.");
    }
    if (!options.calibrationCompiler.empty() && options.calibrationName.empty())
    {
        printErrorMessage("--calibrate requires --name <toolchain-name>.");
    }
    if (!options.calibrationCompiler.empty() && !options.removalName.empty())
    {
        printErrorMessage("--calibrate and --remove-toolchain cannot be combined.");
    }
    if (options.project && options.calibrationCompiler.empty())
    {
        printErrorMessage("--project requires --calibrate <compiler-path-or-name>.");
    }
    if (!options.calibrationCompiler.empty() || !options.removalName.empty())
    {
        if (!options.buildDirectory.empty() || !options.toolchain.empty() || options.defaultJobs != 0 ||
            !options.buildArguments.empty() || options.configureOnly || options.reconfigure || options.listToolchains)
        {
            printErrorMessage("Toolchain registry edits cannot be combined with build options, targets, or "
                              "--list-toolchains.");
        }
    }
    if (options.listToolchains && (!options.toolchain.empty() || options.defaultJobs != 0 ||
                                   !options.buildArguments.empty() || options.configureOnly || options.reconfigure))
    {
        printErrorMessage("--list-toolchains cannot be combined with a build request.");
    }
    return options;
}

void printUsage()
{
    printMessage("Usage: hbuild [options] [targets...] [-- targets...]\n"
                 "       hbuild --calibrate <compiler-path-or-name> --name <toolchain-name> [--project]\n"
                 "       hbuild --remove-toolchain <name>\n"
                 "\n"
                 "Project selection:\n"
                 "  -B <directory>          Select a build directory or its target/configuration subdirectory\n"
                 "  --toolchain <name>      Select the project toolchain\n"
                 "  --list-toolchains       List registered toolchains; -B may select another project\n"
                 "  From the source directory, use: hbuild -B build\n"
                 "  The build root is the first directory below the nearest parent containing hmake.cpp\n"
                 "\n"
                 "Toolchain registry:\n"
                 "  --calibrate <compiler> Inspect and validate a Linux Clang/GCC default profile\n"
                 "  --name <name>          Name the calibrated toolchain (required with --calibrate)\n"
                 "  --remove-toolchain <name> Remove a registry entry; the default cannot be removed\n"
                 "  --project             Save calibration beside hmake.cpp instead of in the user registry\n"
                 "  Removal searches the user registry and the project containing the current directory\n"
                 "  Registry edits need no -B; --project requires a project and --calibrate\n"
                 "  Calibration never overwrites entries; removal never deletes compiler files\n"
                 "\n"
                 "Execution:\n"
                 "  --default-jobs <count>  Persist the project's default job count\n"
                 "  -j <count>              Override the job count for this invocation\n"
                 "  --configure-only        Stop before dispatching the generated build\n"
                 "  --reconfigure           Force configuration\n"
                 "  --recompile             Rebuild both generated executables, then configure\n"
                 "  --dry-run               Print generated build actions without executing them\n"
                 "  --header-units-only     Build only header-unit work\n"
                 "  --standalone            Emit standalone compile scripts\n"
                 "  --print-hash-map        Write the generated hash-map diagnostic\n"
                 "  --help                  Print this help and exit\n");
}

void appendArgument(string &command, const string_view argument)
{
    if (!command.empty())
    {
        command.push_back(' ');
    }
#ifdef _WIN32
    // Native execution bypasses cmd.exe: only CRT quoting is needed, and '%'/'!' remain literal.
    if (!argument.empty() && argument.find_first_of(" \t\r\n\f\v\"") == string_view::npos)
    {
        command += argument;
        return;
    }
    command.push_back('"');
    uint64_t backslashes = 0;
    for (const char character : argument)
    {
        if (character == '\\')
        {
            ++backslashes;
            continue;
        }
        if (character == '"')
        {
            command.append(backslashes * 2 + 1, '\\');
            command.push_back('"');
            backslashes = 0;
            continue;
        }
        command.append(backslashes, '\\');
        backslashes = 0;
        command.push_back(character);
    }
    command.append(backslashes * 2, '\\');
    command.push_back('"');
#else
    if (!argument.empty() && argument.find_first_of(" \t\r\n\f\v'\"\\$`!&;|<>()[]{}*?#~") == string_view::npos)
    {
        command += argument;
        return;
    }
    command.push_back('\'');
    for (const char character : argument)
    {
        if (character == '\'')
        {
            command += "'\\''";
        }
        else
        {
            command.push_back(character);
        }
    }
    command.push_back('\'');
#endif
}

string loadBuildCachePrefix(const path &file)
{
    const string fileName = file.string();
    FILE *const input = std::fopen(fileName.c_str(), "rb");
    if (input == nullptr)
    {
        printErrorMessage("Could not open the build cache: " + fileName + "\nSystem error: " + std::strerror(errno));
    }

    const auto readBytes = [&](void *destination, const uint64_t size) {
        if (std::fread(destination, 1, size, input) != size)
        {
            printErrorMessage("Could not read the complete build-cache prefix: " + fileName);
        }
    };

    uint32_t cachedPrefixSize;
    readBytes(&cachedPrefixSize, sizeof(cachedPrefixSize));
    const uint64_t prefixSize = cachedPrefixSize;
    assert(prefixSize >= sizeof(uint32_t) + 2 * sizeof(uint64_t) + 2 * sizeof(uint32_t));

    string prefix;
    prefix.resize_and_overwrite(prefixSize, [&](char *bytes, const uint64_t) {
        memcpy(bytes, &cachedPrefixSize, sizeof(cachedPrefixSize));
        readBytes(bytes + sizeof(cachedPrefixSize), prefixSize - sizeof(cachedPrefixSize));
        return prefixSize;
    });
    std::fclose(input);
    readBuildCacheInvalidationPrefix(prefix);
    return prefix;
}

void writeBuildCachePrefix(const path &file, const string_view cachedPrefix, const bool preserveOrdinaryTail)
{
    string fileBuffer;
    writeBuildCacheInvalidationPrefix(fileBuffer);

    if (preserveOrdinaryTail && fileBuffer == cachedPrefix)
    {
        return;
    }

    const string fileName = file.string();
    if (preserveOrdinaryTail)
    {
        std::error_code error;
        const uint64_t fileSize = std::filesystem::file_size(file, error);
        if (error)
        {
            printErrorMessage("Could not determine the build cache size: " + fileName +
                              "\nSystem error: " + error.message());
        }
        assert(fileSize >= cachedPrefix.size());
        const uint64_t tailSize = fileSize - cachedPrefix.size();
        if (tailSize == 0)
        {
            writeCacheFile(fileName, fileBuffer);
            return;
        }
        FILE *const tailInput = std::fopen(fileName.c_str(), "rb");
        if (tailInput == nullptr)
        {
            printErrorMessage("Could not open the build cache to preserve its target rows: " + fileName +
                              "\nSystem error: " + std::strerror(errno));
        }
#ifdef _WIN32
        const int seekResult = _fseeki64(tailInput, static_cast<int64_t>(cachedPrefix.size()), SEEK_SET);
#else
        const int seekResult = fseeko(tailInput, static_cast<off_t>(cachedPrefix.size()), SEEK_SET);
#endif
        if (seekResult != 0)
        {
            printErrorMessage("Could not seek to the build-cache target rows: " + fileName +
                              "\nSystem error: " + std::strerror(errno));
        }

        const uint64_t prefixBytes = fileBuffer.size();
        fileBuffer.resize_and_overwrite(prefixBytes + tailSize, [&](char *bytes, const uint64_t) {
            if (std::fread(bytes + prefixBytes, 1, tailSize, tailInput) != tailSize)
            {
                printErrorMessage("Could not read the complete build-cache target rows: " + fileName);
            }
            return prefixBytes + tailSize;
        });
        std::fclose(tailInput);
    }
    writeCacheFile(fileName, fileBuffer);
}

string makeCompileCommand(const Toolchain &toolchain, const bool configureMode, const string_view sourceFile,
                          const string_view outputFile, const string_view dependencyFile, const string_view objectFile)
{
    const string_view staticLibrary = configureMode ? HCONFIGURE_C_STATIC_LIB_PATH : HCONFIGURE_B_STATIC_LIB_PATH;
    constexpr string_view includePaths[] = {HCONFIGURE_HEADER, THIRD_PARTY_HEADER, RAPIDJSON_HEADER};
    string command;
    command.reserve(4 * 1024);
    appendArgument(command, toolchain.compiler.bTPath);
    for (const string &argument : toolchain.bootstrapArguments)
    {
        appendArgument(command, argument);
    }

    if (toolchain.style == "gnu")
    {
        command += " -std=c++23 -O0 -fno-exceptions -fno-rtti -fvisibility=hidden"
                   " -ffunction-sections -fdata-sections -pthread -nostdinc -nostdinc++";
        if (!configureMode)
        {
            command += " -DBUILD_MODE -DNDEBUG";
        }
        for (const string_view include : includePaths)
        {
            command += " -I";
            appendArgument(command, include);
        }
        for (const string &include : toolchain.includeDirs)
        {
            command += " -isystem";
            appendArgument(command, include);
        }
        command += " -MMD -MF";
        appendArgument(command, dependencyFile);
        command += " -MQ";
        appendArgument(command, outputFile);
        appendArgument(command, sourceFile);
        for (const string &directory : toolchain.libraryDirs)
        {
            command += " -L";
            appendArgument(command, directory);
        }
        command += " -Wl,--gc-sections -Wl,--whole-archive";
        appendArgument(command, staticLibrary);
        command += " -Wl,--no-whole-archive -o";
        appendArgument(command, outputFile);
    }
    else
    {
        STACK_PMR_STRING(argument, 4 * 1024)
        const auto appendPrefixed = [&](const string_view prefix, const string_view value) {
            argument.assign(prefix);
            argument += value;
            appendArgument(command, argument);
        };
        command += " /std:c++latest /Od /GR- /EHs-c- /D_HAS_EXCEPTIONS=0 /nologo /X";
#ifdef _DEBUG
        command += " /MTd";
#else
        command += " /MT";
#endif
        if (!configureMode)
        {
            command += " /DBUILD_MODE /DNDEBUG";
        }
        for (const string_view include : includePaths)
        {
            appendPrefixed("/I", include);
        }
        for (const string &include : toolchain.includeDirs)
        {
            appendPrefixed("/I", include);
        }
        command += " /sourceDependencies";
        appendArgument(command, dependencyFile);
        appendArgument(command, sourceFile);
        appendPrefixed("/Fo", objectFile);
        // Keep compiler debug data separate when bootstrap arguments enable /Zi or /ZI.
        argument.assign("/Fd");
        argument += objectFile;
        argument += ".pdb";
        appendArgument(command, argument);
        command += " /link /SUBSYSTEM:CONSOLE /NOLOGO";
        for (const string &directory : toolchain.libraryDirs)
        {
            appendPrefixed("/LIBPATH:", directory);
        }
        appendPrefixed("/WHOLEARCHIVE:", staticLibrary);
        command += " kernel32.lib synchronization.lib user32.lib gdi32.lib winspool.lib shell32.lib"
                   " ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib";
        appendPrefixed("/OUT:", outputFile);
    }
    return command;
}

void runGeneratedConfigure(const path &executable, const string_view buildDirectory, const path &configFile)
{
    string command;
    appendArgument(command, executable.string());
    printMessage("Running configure\n");
    const auto started = std::chrono::steady_clock::now();
    const RunCommand::OutputAndStatus result = RunCommand::runProcess(command, buildDirectory.data(), false);
    const double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    if (!result.output.empty())
    {
        printMessage(result.output);
    }
    printMessage(FORMAT("configure execution time: {:.3f} seconds\n", elapsedSeconds));
    if (result.exitStatus != 0)
    {
        string diagnostic =
            "Generated configure executable failed with exit code " + std::to_string(result.exitStatus) +
            ".\nDelete the build directory and run hbuild again.\nBuild directory: " + string(buildDirectory);
        std::error_code error;
        // Failed configuration may have replaced configuration rows without committing their matching build rows.
        std::filesystem::remove(configFile, error);
        if (error)
        {
            diagnostic += "\nAdditionally, stale metadata could not be invalidated: " + configFile.string() +
                          "\nSystem error: " + error.message();
        }
        printErrorMessage(diagnostic);
    }
}

} // namespace

int main(const int argc, char **argv)
{
    std::error_code invocationError;
    string buildWorkingDirectory = std::filesystem::current_path(invocationError).string();
    if (invocationError)
    {
        printErrorMessage("Could not determine the current directory.\nSystem error: " + invocationError.message());
    }
    lowerCaseOnWindows(buildWorkingDirectory.data(), buildWorkingDirectory.size());

    const Options options = parseOptions(argc, argv);
    if (options.help)
    {
        printUsage();
        return 0;
    }
    if (!options.calibrationCompiler.empty() || !options.removalName.empty())
    {
        constructGlobals();
        std::error_code error;
        const path sourceDirectory = std::filesystem::is_regular_file(path(buildWorkingDirectory) / "hmake.cpp", error)
                                         ? path(buildWorkingDirectory)
                                         : findProjectDirectory(buildWorkingDirectory, false);
        if (options.project && sourceDirectory.empty())
        {
            printErrorMessage("--project requires hmake.cpp in the current directory or one of its parents.");
        }
        toolchains.initialize(sourceDirectory);
        if (!options.removalName.empty())
        {
            toolchains.remove(options.removalName);
        }
        else
        {
            toolchains.calibrate(options.calibrationCompiler, options.calibrationName, options.project);
        }
        destructGlobals();
        return 0;
    }

    std::error_code error;
    if (!options.buildDirectory.empty())
    {
        STACK_PMR_STRING(normalizedPath, 4 * 1024)
        if (!Node::isAbsolute(options.buildDirectory))
        {
            normalizedPath = buildWorkingDirectory;
            normalizedPath += slashc;
        }
        normalizedPath += options.buildDirectory;
        Node::normalize<PathType::ABSOLUTE>(normalizedPath);
        buildWorkingDirectory.assign(normalizedPath);
        std::filesystem::create_directories(buildWorkingDirectory, error);
        if (error)
        {
            printErrorMessage("Could not create build directory: " + buildWorkingDirectory +
                              "\nSystem error: " + error.message());
        }
    }
    const path sourceDirectory = findProjectDirectory(buildWorkingDirectory, false);
    if (sourceDirectory.empty())
    {
        printErrorMessage("Could not find hmake.cpp in any parent directory of the build directory.\n"
                          "Build directory: " +
                          buildWorkingDirectory);
    }

    if (options.listToolchains)
    {
        toolchains.initialize(sourceDirectory);
        string toolchainsJson = toolchains.toJson();
        toolchainsJson.push_back('\n');
        printMessage(toolchainsJson);
        return 0;
    }

    string sourcePath = sourceDirectory.string();
    // Keep the full selected directory for target filtering; all build metadata belongs directly below the source root.
    const path buildDirectoryPath =
        string_view(buildWorkingDirectory).substr(0, buildWorkingDirectory.find(slashc, sourcePath.size() + 1));

    constructGlobals();
    const path bootstrapDirectory = buildDirectoryPath / ".hbuild";
    std::filesystem::create_directories(bootstrapDirectory, error);
    if (error)
    {
        printErrorMessage("Could not create hbuild metadata directory: " + bootstrapDirectory.string() +
                          "\nSystem error: " + error.message());
    }
    FileLock projectLock(bootstrapDirectory / "lock");

    const path cacheFile = buildDirectoryPath / projectCacheFileName;
    const path nodesFile = buildDirectoryPath / nodesCacheFileName;
    const path configCacheFile = buildDirectoryPath / configCacheFileName;
    const path buildCacheFile = buildDirectoryPath / buildCacheFileName;
    path configureExecutable = buildDirectoryPath / "configure";
    path buildExecutable = buildDirectoryPath / "build";
    if constexpr (os == OS::NT)
    {
        configureExecutable += ".exe";
        buildExecutable += ".exe";
    }

    // Validate once, before reading or modifying caches. Only an entirely fresh or complete state can be used.
    const path *const requiredFiles[] = {&cacheFile, &configureExecutable, &buildExecutable,
                                         &nodesFile, &configCacheFile,     &buildCacheFile};
    uint64_t presentCount = 0;
    for (const path *file : requiredFiles)
    {
        const auto status = std::filesystem::status(*file, error);
        if (status.type() == std::filesystem::file_type::not_found)
        {
            continue;
        }
        if (error)
        {
            printErrorMessage("Could not inspect build artifact: " + file->string() +
                              "\nSystem error: " + error.message());
        }
        if (!std::filesystem::is_regular_file(status))
        {
            printErrorMessage("Expected a regular build artifact file: " + file->string());
        }
        ++presentCount;
    }
    if (presentCount != 0 && presentCount != std::size(requiredFiles))
    {
        string diagnostic = "Incomplete build directory: required build artifacts are missing.\n"
                            "Delete the build directory and run hbuild again.\nBuild directory: " +
                            buildDirectoryPath.string() + "\nMissing files:";
        for (const path *file : requiredFiles)
        {
            if (!std::filesystem::is_regular_file(*file, error))
            {
                diagnostic += "\n  " + file->filename().string();
            }
        }
        printErrorMessage(diagnostic);
    }
    const bool freshBuild = presentCount == 0;

    if (!freshBuild)
    {
        string cacheError;
        if (!projectCache.parse(fileToString(cacheFile.string()), cacheError))
        {
            printErrorMessage("Invalid project cache.\nFile: " + cacheFile.string() + "\n" + cacheError);
        }
    }
    if (!options.toolchain.empty())
    {
        projectCache.needsWrite = projectCache.needsWrite || projectCache.toolchainName != options.toolchain;
        projectCache.toolchainName = options.toolchain;
    }
    if (options.defaultJobs != 0)
    {
        projectCache.needsWrite = projectCache.needsWrite || projectCache.defaultJobs != options.defaultJobs;
        projectCache.defaultJobs = options.defaultJobs;
    }

    string hmakePath = sourcePath;
    hmakePath.push_back(slashc);
    hmakePath += "hmake.cpp";
    if (!freshBuild)
    {
        loadNodesCache(nodesFile);
        assert(srcNode->filePath == sourcePath);
        assert(configureNode->filePath == buildDirectoryPath.string());
    }
    else
    {
        srcNode = Node::getHalfNode<PathType::NORMAL_ABSOLUTE>(std::move(sourcePath));
        configureNode = Node::getHalfNode<PathType::NORMAL_ABSOLUTE>(buildDirectoryPath.string());
    }
    normalizationBasePath = srcNode->filePath;
    Node *const hmakeFile = Node::getHalfNode<PathType::NORMAL_ABSOLUTE>(std::move(hmakePath));

    const Toolchain *const bootstrapToolchain = toolchains.registryOrder.front();
    if (projectCache.toolchainName.empty())
    {
        projectCache.toolchainName = bootstrapToolchain->name;
        projectCache.needsWrite = true;
    }

    const string buildCachePrefix = freshBuild ? string{} : loadBuildCachePrefix(buildCacheFile);
    if (!freshBuild && configurationTime == -1)
    {
        printErrorMessage("The previous configuration did not complete.\n"
                          "Delete the build directory and run hbuild again.\nBuild directory: " +
                          buildDirectoryPath.string());
    }

    // Existing build caches already contain the mandatory hmake.cpp input.
    if (recompileNodes.emplace(hmakeFile).second && !freshBuild)
    {
        printErrorMessage("The build cache is missing its hmake.cpp input.\n"
                          "Delete the build directory and run hbuild again.\nBuild directory: " +
                          buildDirectoryPath.string());
    }
    bool mustCompile = options.recompile || freshBuild;
    bool mustConfigure = mustCompile || options.reconfigure || projectCacheContentHash != projectCache.contentCache();

    if (projectCache.needsWrite)
    {
        STACK_PMR_STRING(cacheContents, 4 * 1024)
        string cacheError;
        if (!projectCache.serialize(cacheContents, cacheError))
        {
            printErrorMessage(cacheError);
        }
        writeCacheFile(cacheFile.string(), cacheContents);
        projectCache.needsWrite = false;
    }

    for (Node *node : recompileNodes)
    {
        node->doHashFile = true;
    }
    if (!mustConfigure)
    {
        for (Node *node : reconfigureNodes)
        {
            node->doHashFile = true;
        }
    }
    Builder::checkNodes();
    if (!mustCompile)
    {
        for (Node *node : recompileNodes)
        {
            const auto baseline = recompileBaselineHashes.find(node);
            if (baseline == recompileBaselineHashes.end() || baseline->second != node->contentHash)
            {
                mustCompile = true;
                break;
            }
        }
    }
    mustConfigure = mustConfigure || mustCompile;
    if (!mustConfigure)
    {
        for (Node *node : reconfigureNodes)
        {
            const auto baseline = reconfigureBaselineHashes.find(node);
            if (baseline == reconfigureBaselineHashes.end() || baseline->second != node->contentHash)
            {
                mustConfigure = true;
                break;
            }
        }
    }
    if (mustCompile)
    {
        const string dependencyFiles[] = {(bootstrapDirectory / "configure.d").string(),
                                          (bootstrapDirectory / "build.d").string()};
        const string commands[] = {
            makeCompileCommand(*bootstrapToolchain, true, hmakeFile->filePath, configureExecutable.string(),
                               dependencyFiles[0], (bootstrapDirectory / "configure.obj").string()),
            makeCompileCommand(*bootstrapToolchain, false, hmakeFile->filePath, buildExecutable.string(),
                               dependencyFiles[1], (bootstrapDirectory / "build.obj").string())};
        RunCommand::OutputAndStatus results[2];
        double elapsedSeconds[2];
        const auto compile = [&](const uint64_t index) {
            const auto started = std::chrono::steady_clock::now();
            results[index] = RunCommand::runProcess(commands[index], configureNode->filePath.data());
            elapsedSeconds[index] = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        };
        printMessage("Compiling configure and build executables in parallel\n");
        std::thread configureCompilation(compile, 0);
        compile(1);
        configureCompilation.join();

        // Join both compilations before reporting errors or touching caches: neither child may outlive this step.
        for (uint64_t index = 0; index < std::size(commands); ++index)
        {
            const string_view label = index == 0 ? "configure" : "build";
            if (results[index].exitStatus != 0)
            {
                printErrorMessage(FORMAT("Could not compile the generated {} executable.\nExit code: {}\n"
                                         "Command: {}\nCompiler output:\n{}",
                                         label, results[index].exitStatus, commands[index], results[index].output));
            }
            // Parse on this thread after both compiler processes have finished: Node interning is single-threaded.
            const auto headers =
                parseHeaderDeps(results[index].output, bootstrapToolchain->compiler, results[index].exitStatus,
                                dependencyFiles[index], configureNode->filePath, hmakeFile, false);
            for (Node *node : headers)
            {
                recompileNodes.emplace(node);
                node->doHashFile = true;
            }
            if (!results[index].output.empty())
            {
                printMessage(results[index].output);
            }
            printMessage(FORMAT("{} compilation time: {:.3f} seconds\n", label, elapsedSeconds[index]));
        }

        // Resolve newly discovered headers before committing the successful compilation's input snapshots.
        Builder::checkNodes();
        for (Node *node : recompileNodes)
        {
            recompileBaselineHashes[node] = node->contentHash;
        }
    }

    if (mustConfigure)
    {
        // The generated configure/build executables may extend both sets. Preserve those registrations and commit the
        // next configuration time only after configuration has completed successfully.
        configurationTime = -1;
        writeBuildCachePrefix(buildCacheFile, buildCachePrefix, !freshBuild);
    }

    writeNodesCache();
    if (mustConfigure)
    {
        runGeneratedConfigure(configureExecutable, configureNode->filePath, configCacheFile);
    }

    int result = 0;
    if (!options.configureOnly)
    {
        string command;
        appendArgument(command, buildExecutable.string());
        for (const string_view argument : options.buildArguments)
        {
            appendArgument(command, argument);
        }
        const RunCommand::OutputAndStatus buildResult =
            RunCommand::runProcess(command, buildWorkingDirectory.c_str(), false);
        if (!buildResult.output.empty())
        {
            printMessage(buildResult.output);
        }
        result = buildResult.exitStatus;
    }
    destructGlobals();
    return result;
}
