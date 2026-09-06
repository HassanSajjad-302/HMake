#include "Bootstrap.hpp"

#include "BuildTools.hpp"
#include "Builder.hpp"
#include "Cache.hpp"
#include "Node.hpp"
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

#ifdef _WIN32
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace
{
using std::string;
using std::string_view;
using std::vector;
using std::filesystem::path;

/// Serializes every operation that can replace metadata in one build directory.
/// The handle is deliberately non-inheritable, so configure/build subprocesses do not
/// keep the lock alive after hbuild exits.
class ProjectLock
{
  public:
    ProjectLock() = default;
    ProjectLock(const ProjectLock &) = delete;
    ProjectLock &operator=(const ProjectLock &) = delete;

    ~ProjectLock()
    {
#ifdef _WIN32
        if (handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
        }
#else
        if (descriptor != -1)
        {
            close(descriptor);
        }
#endif
    }

    void acquire(const path &file)
    {
#ifdef _WIN32
        const string fileName = file.string();
        handle = CreateFileA(fileName.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            printErrorMessage("Could not open the project lock: " + fileName +
                              "\nWindows error: " + std::to_string(GetLastError()));
        }
        OVERLAPPED overlapped{};
        if (!LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, MAXDWORD, MAXDWORD,
                        &overlapped))
        {
            printErrorMessage("Could not acquire the project lock: " + fileName +
                              "\nWindows error: " + std::to_string(GetLastError()));
        }
#else
        descriptor = open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0666);
        if (descriptor == -1)
        {
            printErrorMessage("Could not open the project lock: " + file.string() +
                              "\nSystem error: " + std::strerror(errno));
        }
        while (flock(descriptor, LOCK_EX | LOCK_NB) == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            printErrorMessage("Could not acquire the project lock: " + file.string() +
                              "\nSystem error: " + std::strerror(errno));
        }
#endif
    }

  private:
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
#else
    int descriptor = -1;
#endif
};

struct Options
{
    string_view buildDirectory;
    string_view toolchain;
    uint16_t defaultJobs = 0;
    vector<string_view> buildArguments;
    bool configureOnly = false;
    bool reconfigure = false;
    bool recompile = false;
    bool listToolchains = false;
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
                 "\n"
                 "Project selection:\n"
                 "  -B <directory>          Build directory; a parent directory must contain hmake.cpp\n"
                 "  --toolchain <name>      Select the project toolchain\n"
                 "  --list-toolchains       List registered toolchains; -B may select another project\n"
                 "  From the source directory, use: hbuild -B build\n"
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

struct Command
{
    string value;
    // Borrows NUL-terminated storage that must remain valid until run() finishes.
    string_view directory;

    Command(const string_view executable, const string_view workingDirectory, const uint64_t capacity = 0)
        : directory(workingDirectory)
    {
        value.reserve(capacity == 0 ? executable.size() + 256 : capacity);
        appendValue(executable);
    }

    RunCommand::OutputAndStatus run() const
    {
        return RunCommand::runProcess(value, directory.empty() ? nullptr : directory.data());
    }

    void append(const string_view argument)
    {
        value.push_back(' ');
        appendValue(argument);
    }

  private:
    void appendValue(const string_view argument)
    {
#ifdef _WIN32
        // Native execution bypasses cmd.exe: only CRT quoting is needed, and '%'/'!' remain literal.
        if (!argument.empty() && argument.find_first_of(" \t\r\n\f\v\"") == string_view::npos)
        {
            value += argument;
            return;
        }
        value.push_back('"');
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
                value.append(backslashes * 2 + 1, '\\');
                value.push_back('"');
                backslashes = 0;
                continue;
            }
            value.append(backslashes, '\\');
            backslashes = 0;
            value.push_back(character);
        }
        value.append(backslashes * 2, '\\');
        value.push_back('"');
#else
        if (!argument.empty() && argument.find_first_of(" \t\r\n\f\v'\"\\$`!&;|<>()[]{}*?#~") == string_view::npos)
        {
            value += argument;
            return;
        }
        value.push_back('\'');
        for (const char character : argument)
        {
            if (character == '\'')
            {
                value += "'\\''";
            }
            else
            {
                value.push_back(character);
            }
        }
        value.push_back('\'');
#endif
    }
};

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

Command makeCompileCommand(const Toolchain &toolchain, const bool configureMode, const string_view sourceFile,
                           const string_view outputFile, const string_view objectFile,
                           const string_view workingDirectory)
{
    const string_view staticLibrary = configureMode ? HCONFIGURE_C_STATIC_LIB_PATH : HCONFIGURE_B_STATIC_LIB_PATH;
    constexpr string_view includePaths[] = {HCONFIGURE_HEADER, THIRD_PARTY_HEADER, RAPIDJSON_HEADER};
    Command command(toolchain.compiler.bTPath, workingDirectory, 4 * 1024);
    for (const string &argument : toolchain.bootstrapArguments)
    {
        command.append(argument);
    }

    if (toolchain.style == "gnu")
    {
        command.value += " -std=c++23 -O0 -fno-exceptions -fno-rtti -fvisibility=hidden"
                         " -ffunction-sections -fdata-sections -pthread -nostdinc -nostdinc++";
        if (!configureMode)
        {
            command.value += " -DBUILD_MODE -DNDEBUG";
        }
        for (const string_view include : includePaths)
        {
            command.value += " -I";
            command.append(include);
        }
        for (const string &include : toolchain.includeDirs)
        {
            command.value += " -isystem";
            command.append(include);
        }
        command.append(sourceFile);
        for (const string &directory : toolchain.libraryDirs)
        {
            command.value += " -L";
            command.append(directory);
        }
        command.value += " -Wl,--gc-sections -Wl,--whole-archive";
        command.append(staticLibrary);
        command.value += " -Wl,--no-whole-archive -o";
        command.append(outputFile);
    }
    else
    {
        STACK_PMR_STRING(argument, 4 * 1024)
        const auto appendPrefixed = [&](const string_view prefix, const string_view value) {
            argument.assign(prefix);
            argument += value;
            command.append(argument);
        };
        command.value += " /std:c++latest /Od /GR- /EHs-c- /D_HAS_EXCEPTIONS=0 /nologo /X";
#ifdef _DEBUG
        command.value += " /MTd";
#else
        command.value += " /MT";
#endif
        if (!configureMode)
        {
            command.value += " /DBUILD_MODE /DNDEBUG";
        }
        for (const string_view include : includePaths)
        {
            appendPrefixed("/I", include);
        }
        for (const string &include : toolchain.includeDirs)
        {
            appendPrefixed("/I", include);
        }
        command.append(sourceFile);
        appendPrefixed("/Fo", objectFile);
        // Keep compiler debug data separate when bootstrap arguments enable /Zi or /ZI.
        argument.assign("/Fd");
        argument += objectFile;
        argument += ".pdb";
        command.append(argument);
        command.value += " /link /SUBSYSTEM:CONSOLE /NOLOGO";
        for (const string &directory : toolchain.libraryDirs)
        {
            appendPrefixed("/LIBPATH:", directory);
        }
        appendPrefixed("/WHOLEARCHIVE:", staticLibrary);
        command.value += " kernel32.lib synchronization.lib user32.lib gdi32.lib winspool.lib shell32.lib"
                         " ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib";
        appendPrefixed("/OUT:", outputFile);
    }
    return command;
}

void runGeneratedConfigure(const path &executable, const string_view buildDirectory, const path &configFile)
{
    const Command command(executable.string(), buildDirectory);
    printMessage("Running configure\n");
    const auto started = std::chrono::steady_clock::now();
    const RunCommand::OutputAndStatus result = command.run();
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

int runBootstrap(const int argc, char **argv)
{
    std::error_code invocationError;
    string invocationPath = std::filesystem::current_path(invocationError).string();
    if (invocationError)
    {
        printErrorMessage("Could not determine the current directory.\nSystem error: " + invocationError.message());
    }
    lowerCaseOnWindows(invocationPath.data(), invocationPath.size());
    const path invocationDirectory(invocationPath);

    Options options = parseOptions(argc, argv);
    if (options.help)
    {
        printUsage();
        return 0;
    }

    std::error_code error;
    path buildDirectoryPath = invocationDirectory;
    if (!options.buildDirectory.empty())
    {
        STACK_PMR_STRING(normalizedPath, 4 * 1024)
        normalizedPath = options.buildDirectory;
        Node::normalize<PathType::NEITHER>(normalizedPath);
        buildDirectoryPath = normalizedPath;
        std::filesystem::create_directories(buildDirectoryPath, error);
        if (error)
        {
            printErrorMessage("Could not create build directory: " + buildDirectoryPath.string() +
                              "\nSystem error: " + error.message());
        }
    }
    const path sourceDirectory = buildDirectoryPath.has_relative_path()
                                     ? findProjectDirectory(buildDirectoryPath.parent_path(), false)
                                     : path{};
    if (sourceDirectory.empty())
    {
        printErrorMessage("Could not find hmake.cpp in any parent directory of the build directory.\n"
                          "Build directory: " +
                          buildDirectoryPath.string());
    }

    if (options.listToolchains)
    {
        toolchains.initialize(sourceDirectory);
        string toolchainsJson = toolchains.toJson();
        toolchainsJson.push_back('\n');
        printMessage(toolchainsJson);
        return 0;
    }

    constructGlobals();
    ProjectLock projectLock;
    const path bootstrapDirectory = buildDirectoryPath / ".hbuild";
    std::filesystem::create_directories(bootstrapDirectory, error);
    if (error)
    {
        printErrorMessage("Could not create hbuild metadata directory: " + bootstrapDirectory.string() +
                          "\nSystem error: " + error.message());
    }
    projectLock.acquire(bootstrapDirectory / "lock");

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
                                        &nodesFile, &configCacheFile, &buildCacheFile};
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

    string sourcePath = sourceDirectory.string();
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
    Node *const projectCacheFile = Node::getHalfNode<PathType::NORMAL_ABSOLUTE>(cacheFile.string());

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

    // Keep each insertion first: both inputs must be registered even when rebuilding is already required.
    bool mustCompile = recompileNodes.emplace(hmakeFile).second || options.recompile || freshBuild;
    bool mustConfigure = reconfigureNodes.emplace(projectCacheFile).second || mustCompile || projectCache.needsWrite ||
                         options.reconfigure;

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

    if (mustCompile)
    {
        for (Node *node : recompileNodes)
        {
            node->doHashFile = true;
        }
        Builder::checkNodes();
    }
    else
    {
        STACK_PMR_VECTOR(uint64_t, cachedSnapshots, 128)
        cachedSnapshots.reserve(recompileNodes.size() * 2);
        for (Node *node : recompileNodes)
        {
            node->doHashFile = true;
            cachedSnapshots.emplace_back(node->lastWriteTime);
            cachedSnapshots.emplace_back(node->contentHash);
        }
        if (!mustConfigure)
        {
            for (Node *node : reconfigureNodes)
            {
                node->doStatFile = true;
            }
        }
        Builder::checkNodes();
        uint64_t snapshotIndex = 0;
        for (const Node *node : recompileNodes)
        {
            if (node->lastWriteTime != cachedSnapshots[snapshotIndex] ||
                node->contentHash != cachedSnapshots[snapshotIndex + 1])
            {
                mustCompile = true;
                break;
            }
            snapshotIndex += 2;
        }
        mustConfigure = mustConfigure || mustCompile;
        if (!mustConfigure)
        {
            for (const Node *node : reconfigureNodes)
            {
                if (node->fileType != std::filesystem::file_type::regular || node->lastWriteTime > configurationTime)
                {
                    mustConfigure = true;
                    break;
                }
            }
        }
    }
    if (mustCompile)
    {
        const Command commands[] = {
            makeCompileCommand(*bootstrapToolchain, true, hmakeFile->filePath, configureExecutable.string(),
                               (bootstrapDirectory / "configure.obj").string(), configureNode->filePath),
            makeCompileCommand(*bootstrapToolchain, false, hmakeFile->filePath, buildExecutable.string(),
                               (bootstrapDirectory / "build.obj").string(), configureNode->filePath)};
        RunCommand::OutputAndStatus results[2];
        double elapsedSeconds[2];
        const auto compile = [&](const uint64_t index) {
            const auto started = std::chrono::steady_clock::now();
            results[index] = commands[index].run();
            elapsedSeconds[index] =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
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
                                         label, results[index].exitStatus, commands[index].value,
                                         results[index].output));
            }
            if (!results[index].output.empty())
            {
                printMessage(results[index].output);
            }
            printMessage(FORMAT("{} compilation time: {:.3f} seconds\n", label, elapsedSeconds[index]));
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
        const string_view buildDirectory = configureNode->filePath;
        const string_view buildWorkingDirectory =
            invocationPath == buildDirectory || isPathInDirectory(invocationPath, buildDirectory)
                ? string_view(invocationPath)
                : buildDirectory;
        Command command(buildExecutable.string(), buildWorkingDirectory);
        for (const string_view argument : options.buildArguments)
        {
            command.append(argument);
        }
        const RunCommand::OutputAndStatus buildResult = command.run();
        if (!buildResult.output.empty())
        {
            printMessage(buildResult.output);
        }
        result = buildResult.exitStatus;
    }
    destructGlobals();
    return result;
}
