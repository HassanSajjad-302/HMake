#include "Bootstrap.hpp"

#include "BuildTools.hpp"
#include "Builder.hpp"
#include "Cache.hpp"
#include "Node.hpp"
#include "ParseHeaderDeps.hpp"
#include "RunCommand.hpp"
#include "Toolchains.hpp"
#include "rapidhash/rapidhash.h"

#include <cassert>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
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
            const string error = "Could not acquire the project lock: " + fileName +
                                 "\nWindows error: " + std::to_string(GetLastError());
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
            printErrorMessage(error);
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
            const string error =
                "Could not acquire the project lock: " + file.string() + "\nSystem error: " + std::strerror(errno);
            close(descriptor);
            descriptor = -1;
            printErrorMessage(error);
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
    string_view sourceDirectory;
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
    const auto parsePositiveInteger = [](const string_view option, const string_view value) {
        uint16_t parsed = 0;
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (error != std::errc{} || end != value.data() + value.size() || parsed == 0)
        {
            printErrorMessage("Expected an integer from 1 through 65535 after " + string(option) + ": " +
                              string(value));
        }
        return parsed;
    };

    vector<string_view> targets;
    for (int index = 1; index < argc; ++index)
    {
        const string_view argument(argv[index]);
        if (argument == "--")
        {
            while (++index < argc)
            {
                targets.emplace_back(argv[index]);
            }
            break;
        }
        if (argument == "--help")
        {
            options.help = true;
            continue;
        }
        if (argument == "--list-toolchains")
        {
            options.listToolchains = true;
            continue;
        }
        if (argument == "-S" || argument == "-B" || argument == "--toolchain" || argument == "--default-jobs" ||
            argument == "-j")
        {
            const string_view value = takeValue(index, argument);
            if (argument == "-S")
            {
                options.sourceDirectory = value;
            }
            else if (argument == "-B")
            {
                options.buildDirectory = value;
            }
            else if (argument == "--toolchain")
            {
                options.toolchain = value;
            }
            else
            {
                const uint16_t parsed = parsePositiveInteger(argument, value);
                if (argument == "--default-jobs")
                {
                    options.defaultJobs = parsed;
                }
                else
                {
                    options.buildArguments.emplace_back("--jobs");
                    options.buildArguments.emplace_back(value);
                }
            }
            continue;
        }
        if (argument == "--configure-only")
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
        else if (argument.starts_with('-'))
        {
            printErrorMessage("Unknown hbuild option: " + string(argument));
        }
        else
        {
            targets.emplace_back(argument);
        }
    }

    if (!targets.empty())
    {
        options.buildArguments.emplace_back("--");
        for (const string_view target : targets)
        {
            options.buildArguments.emplace_back(target);
        }
    }
    if (options.listToolchains &&
        (!options.buildDirectory.empty() || !options.toolchain.empty() || options.defaultJobs != 0 ||
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
                 "  -S <directory>          Source directory; must be the build directory's immediate parent\n"
                 "  -B <directory>          Build directory; must be an immediate child of the source directory\n"
                 "  --toolchain <name>      Select the project toolchain\n"
                 "  --list-toolchains       List registered toolchains and exit\n"
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

string normalizePath(const string_view input)
{
    STACK_PMR_STRING(result, 4 * 1024)
    result.assign(input);
    Node::normalize<PathType::NEITHER>(result);
    return string(result);
}

bool isRegularFile(const path &file)
{
    std::error_code error;
    return std::filesystem::is_regular_file(file, error) && !error;
}

struct Command
{
    string value;

    Command(const string_view executable, const string_view workingDirectory, const uint64_t capacity = 0)
    {
#ifdef _WIN32
        value = "cd /d ";
#else
        value = "cd ";
#endif
        value.reserve(capacity == 0 ? executable.size() + workingDirectory.size() + 256 : capacity);
        appendValue(workingDirectory);
        value += " && ";
        appendValue(executable);
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
        // cmd.exe parses this value before the child C runtime. Quote empty arguments and cmd metacharacters;
        // doubling backslashes before quotes preserves the final child argv.
        if (!argument.empty() && argument.find_first_of(" \t\r\n\f\v\"&|<>()^%!") == string_view::npos)
        {
            value.append(argument);
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
            value.append(argument);
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

struct BuildCachePrefix
{
    string bytes;
    uint64_t ordinaryTailSize = 0;
};

BuildCachePrefix loadBuildCachePrefix(const path &file)
{
    BuildCachePrefix prefix;
    std::error_code error;
    const uint64_t fileSize = std::filesystem::file_size(file, error);
    if (error == std::errc::no_such_file_or_directory)
    {
        return prefix;
    }
    if (error)
    {
        printErrorMessage("Could not determine the build cache size: " + file.string() +
                          "\nSystem error: " + error.message());
    }
    const string fileName = file.string();
    FILE *input = std::fopen(fileName.c_str(), "rb");
    if (input == nullptr)
    {
        printErrorMessage("Could not open the build cache: " + fileName + "\nSystem error: " + std::strerror(errno));
    }

    prefix.bytes.reserve(4 * 1024);
    const auto readBytes = [&](const uint64_t size) {
        const uint64_t offset = prefix.bytes.size();
        if (size > fileSize - offset)
        {
            return false;
        }
        prefix.bytes.resize(offset + size);
        errno = 0;
        if (size != 0 && std::fread(prefix.bytes.data() + offset, 1, size, input) != size)
        {
            if (std::ferror(input))
            {
                const int readError = errno;
                printErrorMessage("Could not read the build cache: " + fileName + "\nSystem error: " +
                                  (readError == 0 ? string("I/O error") : std::strerror(readError)));
            }
            return false;
        }
        return true;
    };

    uint64_t fixedPrefix[4];
    bool parsed = readBytes(sizeof(fixedPrefix));
    if (parsed)
    {
        memcpy(fixedPrefix, prefix.bytes.data(), sizeof(fixedPrefix));
    }
    auto readIds = [&](flat_hash_set<Node *> &nodes) {
        const uint64_t countOffset = prefix.bytes.size();
        if (!readBytes(sizeof(uint32_t)))
        {
            return false;
        }
        uint32_t count;
        memcpy(&count, prefix.bytes.data() + countOffset, sizeof(count));
        if (count > nodeIndices.size())
        {
            return false;
        }
        const uint64_t idsSize = static_cast<uint64_t>(count) * sizeof(uint32_t);
        const uint64_t idsOffset = prefix.bytes.size();
        if (!readBytes(idsSize))
        {
            return false;
        }
        nodes.reserve(count);
        for (uint32_t index = 0; index < count; ++index)
        {
            uint32_t id;
            memcpy(&id, prefix.bytes.data() + idsOffset + index * sizeof(id), sizeof(id));
            if (id >= nodeIndices.size())
            {
                return false;
            }
            if (!nodes.emplace(Node::getHalfNode(id)).second)
            {
                return false;
            }
        }
        return true;
    };
    parsed = parsed && readIds(recompileNodes) && readIds(reconfigureNodes);
    if (!parsed)
    {
        recompileNodes.clear();
        reconfigureNodes.clear();
    }
    std::fclose(input);
    if (!parsed)
    {
        return {};
    }

    buildExeCommandHash = fixedPrefix[0];
    configureExeCommandHash = fixedPrefix[1];
    selectedToolchainCommandCache = fixedPrefix[2];
    projectCacheContentCache = fixedPrefix[3];
    prefix.ordinaryTailSize = fileSize - prefix.bytes.size();
    return prefix;
}

void writeBuildCachePrefix(const path &file, const BuildCachePrefix &prefix, const bool preserveOrdinaryTail)
{
    const uint64_t prefixSize = 4 * sizeof(uint64_t) + 2 * sizeof(uint32_t) +
                                (recompileNodes.size() + reconfigureNodes.size()) * sizeof(uint32_t);
    const uint64_t tailSize = preserveOrdinaryTail ? prefix.ordinaryTailSize : 0;
    string fileBuffer;
    fileBuffer.reserve(prefixSize + tailSize);
    writeUint64(fileBuffer, buildExeCommandHash);
    writeUint64(fileBuffer, configureExeCommandHash);
    writeUint64(fileBuffer, selectedToolchainCommandCache);
    writeUint64(fileBuffer, projectCacheContentCache);
    uint64_t cachedNodeOffset = 4 * sizeof(uint64_t);
    const auto writeNodes = [&](const flat_hash_set<Node *> &nodes) {
        if (!prefix.bytes.empty())
        {
            const uint64_t recordStart = cachedNodeOffset;
            uint32_t cachedCount;
            memcpy(&cachedCount, prefix.bytes.data() + cachedNodeOffset, sizeof(cachedCount));
            cachedNodeOffset += sizeof(cachedCount);
            bool sameNodes = cachedCount == nodes.size();
            for (uint32_t index = 0; index < cachedCount; ++index)
            {
                uint32_t nodeId;
                memcpy(&nodeId, prefix.bytes.data() + cachedNodeOffset, sizeof(nodeId));
                cachedNodeOffset += sizeof(nodeId);
                sameNodes = sameNodes && nodes.contains(nodeIndices[nodeId]);
            }
            if (sameNodes)
            {
                fileBuffer.append(prefix.bytes.data() + recordStart, cachedNodeOffset - recordStart);
                return;
            }
        }
        writeUint32(fileBuffer, static_cast<uint32_t>(nodes.size()));
        for (const Node *node : nodes)
        {
            assert(node != nullptr && node->myId < nodeIndices.size() && nodeIndices[node->myId] == node);
            writeUint32(fileBuffer, node->myId);
        }
    };
    writeNodes(recompileNodes);
    writeNodes(reconfigureNodes);
    assert(fileBuffer.size() == prefixSize);

    if (fileBuffer == prefix.bytes && (preserveOrdinaryTail || prefix.ordinaryTailSize == 0))
    {
        return;
    }

    if (tailSize != 0)
    {
        const string fileName = file.string();
        FILE *const tailInput = std::fopen(fileName.c_str(), "rb");
        if (tailInput == nullptr)
        {
            printErrorMessage("Could not open the build cache to preserve its target rows: " + fileName +
                              "\nSystem error: " + std::strerror(errno));
        }
        errno = 0;
#ifdef _WIN32
        const int seekResult = _fseeki64(tailInput, static_cast<int64_t>(prefix.bytes.size()), SEEK_SET);
#else
        const int seekResult = fseeko(tailInput, static_cast<off_t>(prefix.bytes.size()), SEEK_SET);
#endif
        if (seekResult != 0)
        {
            const int seekError = errno;
            const string errorMessage = seekError == 0 ? "I/O error" : std::strerror(seekError);
            std::fclose(tailInput);
            printErrorMessage("Could not seek to the build-cache target rows: " + fileName +
                              "\nSystem error: " + errorMessage);
        }

        const uint64_t prefixBytes = fileBuffer.size();
        fileBuffer.resize(prefixBytes + tailSize);
        errno = 0;
        const uint64_t tailBytesRead = std::fread(fileBuffer.data() + prefixBytes, 1, tailSize, tailInput);
        const int tailReadError = errno;
        const bool readFailed = tailBytesRead != tailSize;
        const bool streamError = std::ferror(tailInput);
        std::fclose(tailInput);
        if (readFailed)
        {
            string error = "Could not read the complete build-cache target rows: " + fileName +
                           "\nExpected bytes: " + std::to_string(tailSize) +
                           "\nRead bytes: " + std::to_string(tailBytesRead);
            if (streamError)
            {
                error += "\nSystem error: " + (tailReadError == 0 ? string("I/O error") : std::strerror(tailReadError));
            }
            printErrorMessage(error);
        }
    }
    writeCacheFile(file.string(), fileBuffer);
}

uint64_t toolchainCommandCache(const Toolchain &toolchain)
{
    STACK_PMR_STRING(fingerprint, 16 * 1024)
    fingerprint.append(toolchain.name);
    fingerprint.push_back('\0');
    fingerprint.append(toolchain.family);
    fingerprint.push_back('\0');
    fingerprint.append(toolchain.style);
    fingerprint.push_back('\0');
    fingerprint.append(toolchain.version);
    fingerprint.push_back('\0');
    fingerprint.append(toolchain.target);
    fingerprint.push_back('\0');
    fingerprint.append(toolchain.compiler.bTPath);
    fingerprint.push_back('\0');
    fingerprint.append(toolchain.linker.bTPath);
    fingerprint.push_back('\0');
    fingerprint.append(toolchain.archiver.bTPath);
    for (const string &directory : toolchain.includeDirs)
    {
        fingerprint.push_back('\0');
        fingerprint.push_back('I');
        fingerprint += directory;
    }
    for (const string &directory : toolchain.libraryDirs)
    {
        fingerprint.push_back('\0');
        fingerprint.push_back('L');
        fingerprint += directory;
    }
    for (const string &argument : toolchain.bootstrapArguments)
    {
        fingerprint.push_back('\0');
        fingerprint.push_back('A');
        fingerprint += argument;
    }
    return rapidhash(fingerprint.data(), fingerprint.size());
}

const Toolchain &resolveToolchain(const string_view requested)
{
    const string_view selected = requested.empty() ? toolchains.defaultName() : requested;
    if (selected.empty())
    {
        printErrorMessage("No toolchain is registered.");
    }
    const Toolchain *resolved = toolchains.find(selected);
    if (resolved == nullptr)
    {
        printErrorMessage("Unknown toolchain: " + string(selected) + "\nUse --list-toolchains to list valid names.");
    }
    return *resolved;
}

string makeCompileCommand(const Toolchain &toolchain, const bool configureMode, const string_view sourceFile,
                          const string_view outputFile, const string_view dependencyFile, const string_view objectFile,
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
        constexpr string_view common[] = {"-std=c++23",          "-O0",
                                          "-fno-exceptions",     "-fno-rtti",
                                          "-fvisibility=hidden", "-ffunction-sections",
                                          "-fdata-sections",     "-pthread",
                                          "-nostdinc",           "-nostdinc++"};
        for (const string_view argument : common)
        {
            command.append(argument);
        }
        if (!configureMode)
        {
            command.append("-DBUILD_MODE");
            command.append("-DNDEBUG");
        }
        for (const string_view include : includePaths)
        {
            command.append("-I");
            command.append(include);
        }
        for (const string &include : toolchain.includeDirs)
        {
            command.append("-isystem");
            command.append(include);
        }
        command.append("-MMD");
        command.append("-MF");
        command.append(dependencyFile);
        command.append("-MT");
        command.append(outputFile);
        command.append(sourceFile);
        for (const string &directory : toolchain.libraryDirs)
        {
            command.append("-L");
            command.append(directory);
        }
        command.append("-Wl,--gc-sections");
        command.append("-Wl,--whole-archive");
        command.append(staticLibrary);
        command.append("-Wl,--no-whole-archive");
        command.append("-o");
        command.append(outputFile);
    }
    else
    {
        STACK_PMR_STRING(argument, 4 * 1024)
        const auto appendPrefixed = [&](const string_view prefix, const string_view value) {
            argument.assign(prefix);
            argument.append(value);
            command.append(argument);
        };
        command.append("/std:c++latest");
        command.append("/O0");
        command.append("/GR-");
        command.append("/EHs-c-");
        command.append("/D_HAS_EXCEPTIONS=0");
        command.append("/MT");
        command.append("/nologo");
        command.append("/X");
        if (!configureMode)
        {
            command.append("/DBUILD_MODE");
            command.append("/DNDEBUG");
        }
        for (const string_view include : includePaths)
        {
            appendPrefixed("/I", include);
        }
        for (const string &include : toolchain.includeDirs)
        {
            appendPrefixed("/I", include);
        }
        command.append("/sourceDependencies");
        command.append(dependencyFile);
        command.append(sourceFile);
        appendPrefixed("/Fo", objectFile);
        command.append("/link");
        command.append("/SUBSYSTEM:CONSOLE");
        command.append("/NOLOGO");
        for (const string &directory : toolchain.libraryDirs)
        {
            appendPrefixed("/LIBPATH:", directory);
        }
        appendPrefixed("/WHOLEARCHIVE:", staticLibrary);
        command.append("kernel32.lib");
        command.append("synchronization.lib");
        command.append("user32.lib");
        command.append("gdi32.lib");
        command.append("winspool.lib");
        command.append("shell32.lib");
        command.append("ole32.lib");
        command.append("oleaut32.lib");
        command.append("uuid.lib");
        command.append("comdlg32.lib");
        command.append("advapi32.lib");
        appendPrefixed("/OUT:", outputFile);
    }
    return std::move(command.value);
}

struct CompileTask
{
    string label;
    path executable;
    path dependencyFile;
    string command;
    uint64_t commandHash = 0;
};

flat_hash_set<Node *> compileBootstrapExecutables(const CompileTask &configureTask, const CompileTask &buildTask,
                                                  const Compiler &compiler, const Node *compiledSource)
{
    const auto execute = [&](const CompileTask &task) {
        const auto started = std::chrono::steady_clock::now();
        RunCommand::OutputAndStatus result = RunCommand::runProcess(task.command);
        const double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        if (result.exitStatus != 0)
        {
            printErrorMessage("Could not compile the generated " + task.label +
                              " executable.\nExit code: " + std::to_string(result.exitStatus) +
                              "\nCommand: " + task.command + "\nCompiler output:\n" + result.output);
        }
        if (!result.output.empty())
        {
            printMessage(result.output);
        }
        printMessage(FORMAT("{} compilation time: {:.3f} seconds\n", task.label, elapsedSeconds));
        return parseHeaderDeps(result.output, compiler, result.exitStatus, task.dependencyFile.string(),
                               configureNode->filePath, compiledSource, false);
    };
    flat_hash_set<Node *> dependencies = execute(configureTask);
    flat_hash_set<Node *> buildDependencies = execute(buildTask);
    dependencies.insert(buildDependencies.begin(), buildDependencies.end());
    return dependencies;
}

Node *resolveProject(const Options &options, const path &invocationDirectory, ProjectLock &projectLock)
{
    std::error_code error;
    path buildDirectory;
    if (!options.buildDirectory.empty())
    {
        buildDirectory = normalizePath(options.buildDirectory);
    }
    else if (const path cachedBuild = findProjectBuildDirectory(invocationDirectory); !cachedBuild.empty())
    {
        buildDirectory = cachedBuild;
    }
    else
    {
        if (isRegularFile(invocationDirectory / "hmake.cpp"))
        {
            printErrorMessage("The current directory is a source directory. Select an immediate-child build "
                              "directory with -B <directory>.\nSource directory: " +
                              invocationDirectory.string());
        }
        buildDirectory = invocationDirectory;
    }
    const path sourceDirectory = buildDirectory.parent_path();
    if (sourceDirectory.empty() || sourceDirectory == buildDirectory || sourceDirectory == sourceDirectory.root_path())
    {
        printErrorMessage("The build directory must have a non-root parent source directory.\nBuild directory: " +
                          buildDirectory.string());
    }
    if (!options.sourceDirectory.empty())
    {
        const path requestedSourceDirectory = normalizePath(options.sourceDirectory);
        if (requestedSourceDirectory != sourceDirectory)
        {
            printErrorMessage("The build directory must be an immediate child of the selected source directory.\n"
                              "Selected source directory: " +
                              requestedSourceDirectory.string() + "\nBuild directory: " + buildDirectory.string());
        }
    }
    if (!isRegularFile(sourceDirectory / "hmake.cpp"))
    {
        printErrorMessage("The build directory's immediate parent must contain hmake.cpp.\n"
                          "Source directory: " +
                          sourceDirectory.string() + "\nBuild directory: " + buildDirectory.string());
    }
    const path bootstrapDirectory = buildDirectory / ".hbuild";
    std::filesystem::create_directories(bootstrapDirectory, error);
    if (error)
    {
        printErrorMessage("Could not create hbuild metadata directory: " + bootstrapDirectory.string() +
                          "\nSystem error: " + error.message());
    }
    projectLock.acquire(bootstrapDirectory / "lock");

    const path cacheFile = buildDirectory / projectCacheFileName;
    if (isRegularFile(cacheFile))
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

    toolchains.initialize(sourceDirectory);
    const path nodesFile = buildDirectory / nodesCacheFileName;
    string sourcePath = sourceDirectory.string();
    string configurePath = buildDirectory.string();
    string hmakePath = sourcePath;
    hmakePath.push_back(slashc);
    hmakePath.append("hmake.cpp");
    if (isRegularFile(nodesFile))
    {
        loadNodesCache(nodesFile);
        if (srcNode->filePath != sourcePath || configureNode->filePath != configurePath)
        {
            nodeAllFiles.clear();
            nodeIndices.clear();
            nodeStrings.clear();
            Node::idCount = 0;
            nodesCountBefore = 0;
        }
    }
    if (nodesCountBefore == 0)
    {
        srcNode = Node::getHalfNode<PathType::NORMAL_ABSOLUTE>(std::move(sourcePath));
        configureNode = Node::getHalfNode<PathType::NORMAL_ABSOLUTE>(std::move(configurePath));
        normalizationBasePath = srcNode->filePath;
    }

    return Node::getHalfNode<PathType::NORMAL_ABSOLUTE>(std::move(hmakePath));
}

void runGeneratedConfigure(const path &executable, const path &buildDirectory, const path &configFile)
{
    const Command command(executable.string(), buildDirectory.string());
    printMessage("Running configure\n");
    const auto started = std::chrono::steady_clock::now();
    const RunCommand::OutputAndStatus result = RunCommand::runProcess(command.value);
    const double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    if (!result.output.empty())
    {
        printMessage(result.output);
    }
    printMessage(FORMAT("configure execution time: {:.3f} seconds\n", elapsedSeconds));
    if (result.exitStatus != 0)
    {
        string diagnostic =
            "Generated configure executable failed with exit code " + std::to_string(result.exitStatus) + ".";
        std::error_code error;
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
    normalizationBasePath = invocationPath;
    const path invocationDirectory(invocationPath);

    Options options = parseOptions(argc, argv);
    if (options.help)
    {
        printUsage();
        return 0;
    }

    if (options.listToolchains)
    {
        path sourceDirectory =
            options.sourceDirectory.empty() ? invocationDirectory : path(normalizePath(options.sourceDirectory));
        bool hmakeExists = isRegularFile(sourceDirectory / "hmake.cpp");
        if (!hmakeExists && options.sourceDirectory.empty())
        {
            sourceDirectory = invocationDirectory.parent_path();
            hmakeExists = isRegularFile(sourceDirectory / "hmake.cpp");
        }
        if (!hmakeExists)
        {
            printErrorMessage("Could not find hmake.cpp in the selected source directory.\nSource directory: " +
                              sourceDirectory.string());
        }
        toolchains.initialize(sourceDirectory);
        string toolchainsJson = toolchains.toJson();
        toolchainsJson.push_back('\n');
        printMessage(toolchainsJson);
        return 0;
    }

    constructGlobals();
    ProjectLock projectLock;
    Node *const hmakeFile = resolveProject(options, invocationDirectory, projectLock);

    const Toolchain &bootstrapToolchain = resolveToolchain({});
    const Toolchain &projectToolchain =
        projectCache.toolchainName.empty() || projectCache.toolchainName == bootstrapToolchain.name
            ? bootstrapToolchain
            : resolveToolchain(projectCache.toolchainName);
    const path buildDirectoryPath(configureNode->filePath);
    projectCache.needsWrite = projectCache.needsWrite || projectCache.toolchainName != projectToolchain.name;
    projectCache.toolchainName = projectToolchain.name;
    if (projectCache.needsWrite)
    {
        STACK_PMR_STRING(cacheContents, 4 * 1024)
        string cacheError;
        if (!projectCache.serialize(cacheContents, cacheError))
        {
            printErrorMessage(cacheError);
        }
        writeCacheFile((buildDirectoryPath / projectCacheFileName).string(), cacheContents);
        projectCache.needsWrite = false;
    }

    const path bootstrapDirectory = buildDirectoryPath / ".hbuild";
    const auto makeTask = [&](const bool configureMode) {
        CompileTask task;
        task.label = configureMode ? "configure" : "build";
        task.executable = buildDirectoryPath / task.label;
        if constexpr (os == OS::NT)
        {
            task.executable += ".exe";
        }
        task.dependencyFile = bootstrapDirectory / (task.label + (bootstrapToolchain.style == "msvc" ? ".json" : ".d"));
        const string objectFile =
            bootstrapToolchain.style == "msvc" ? (bootstrapDirectory / (task.label + ".obj")).string() : string{};
        task.command =
            makeCompileCommand(bootstrapToolchain, configureMode, hmakeFile->filePath, task.executable.string(),
                               task.dependencyFile.string(), objectFile, configureNode->filePath);
        task.commandHash = rapidhash(task.command.data(), task.command.size());
        return task;
    };
    CompileTask configureTask = makeTask(true);
    CompileTask buildTask = makeTask(false);

    const path configFile = buildDirectoryPath / configCacheFileName;
    const path buildCacheFile = buildDirectoryPath / buildCacheFileName;
    BuildCachePrefix prefix = nodesCountBefore == 0 ? BuildCachePrefix{} : loadBuildCachePrefix(buildCacheFile);

    const bool metadataMissing = nodesCountBefore == 0 || prefix.bytes.empty();
    const bool configExistsInitially = isRegularFile(configFile);
    const uint64_t selectedToolchainCache = toolchainCommandCache(projectToolchain);
    const uint64_t projectContentCache = projectCache.contentCache();
    bool mustCompile = options.recompile || !isRegularFile(configureTask.executable) ||
                       !isRegularFile(buildTask.executable) || metadataMissing;
    bool mustConfigure = options.reconfigure || mustCompile || !configExistsInitially;

    if (!prefix.bytes.empty())
    {
        if (mustCompile)
        {
            // Configuration replaces this set without hashing it. Keep its current snapshot fresh so the completed
            // configuration is not followed by one redundant reconfiguration on the next hbuild invocation.
            for (Node *node : reconfigureNodes)
            {
                node->doHashFile = true;
            }
        }
        else
        {
            STACK_PMR_VECTOR(uint64_t, cachedSnapshots, 128)
            cachedSnapshots.reserve((recompileNodes.size() + reconfigureNodes.size()) * 2);
            const auto snapshotNodes = [&](const flat_hash_set<Node *> &nodes) {
                for (Node *node : nodes)
                {
                    node->doHashFile = true;
                    cachedSnapshots.emplace_back(node->lastWriteTime);
                    cachedSnapshots.emplace_back(node->contentHash);
                }
            };
            snapshotNodes(recompileNodes);
            snapshotNodes(reconfigureNodes);
            Builder::checkNodes();
            uint64_t snapshotIndex = 0;
            const auto nodesChanged = [&](const flat_hash_set<Node *> &nodes) {
                bool changed = false;
                for (const Node *node : nodes)
                {
                    changed |= node->lastWriteTime != cachedSnapshots[snapshotIndex++];
                    changed |= node->contentHash != cachedSnapshots[snapshotIndex++];
                }
                return changed;
            };
            const bool recompileChanged = nodesChanged(recompileNodes);
            const bool reconfigureChanged = nodesChanged(reconfigureNodes);
            assert(snapshotIndex == cachedSnapshots.size());
            const bool commandCacheChanged =
                buildExeCommandHash != buildTask.commandHash || configureExeCommandHash != configureTask.commandHash;
            const bool projectInputsChanged = selectedToolchainCommandCache != selectedToolchainCache ||
                                              projectCacheContentCache != projectContentCache;
            mustCompile = commandCacheChanged || recompileChanged;
            mustConfigure = mustConfigure || mustCompile || projectInputsChanged || reconfigureChanged;
        }
    }
    if (metadataMissing || options.reconfigure)
    {
        std::error_code error;
        std::filesystem::remove(configFile, error);
        if (error)
        {
            printErrorMessage("Could not invalidate stale metadata file: " + configFile.string() +
                              "\nSystem error: " + error.message());
        }
    }

    if (mustCompile)
    {
        printMessage("Compiling configure and build executables\n");
        recompileNodes = compileBootstrapExecutables(configureTask, buildTask, bootstrapToolchain.compiler, hmakeFile);
        recompileNodes.emplace(hmakeFile);
        recompileNodes.emplace(Node::getNode<PathType::NEITHER>(HCONFIGURE_C_STATIC_LIB_PATH, true));
        recompileNodes.emplace(Node::getNode<PathType::NEITHER>(HCONFIGURE_B_STATIC_LIB_PATH, true));
        Node *const compilerNode = Node::getNode<PathType::NEITHER>(bootstrapToolchain.compiler.bTPath, true, true);
        if (compilerNode->fileType == std::filesystem::file_type::regular)
        {
            recompileNodes.emplace(compilerNode);
        }
    }

    buildExeCommandHash = buildTask.commandHash;
    configureExeCommandHash = configureTask.commandHash;
    selectedToolchainCommandCache = selectedToolchainCache;
    projectCacheContentCache = projectContentCache;

    // Configuration reconstructs this user-owned set directly from the current hmake.cpp. Clear the previous
    // snapshot only after hbuild has used it for the take-off decision, so removed entries do not live forever.
    if (mustConfigure)
    {
        reconfigureNodes.clear();
    }

    if (mustCompile)
    {
        for (Node *node : recompileNodes)
        {
            node->doHashFile = true;
        }
        Builder::checkNodes();
    }
    writeNodesCache();
    if (mustConfigure)
    {
        const bool preserveOrdinaryTail = configExistsInitially && !options.reconfigure && !metadataMissing;
        writeBuildCachePrefix(buildCacheFile, prefix, preserveOrdinaryTail);
        runGeneratedConfigure(configureTask.executable, buildDirectoryPath, configFile);
    }

    int result = 0;
    if (!options.configureOnly)
    {
        const string_view buildDirectory = configureNode->filePath;
        const string_view buildWorkingDirectory =
            invocationPath == buildDirectory || isPathInDirectory(invocationPath, buildDirectory)
                ? string_view(invocationPath)
                : buildDirectory;
        const string buildExecutable = buildTask.executable.string();
        Command command(buildExecutable, buildWorkingDirectory);
        for (const string_view argument : options.buildArguments)
        {
            command.append(argument);
        }
        const RunCommand::OutputAndStatus buildResult = RunCommand::runProcess(command.value);
        if (!buildResult.output.empty())
        {
            printMessage(buildResult.output);
        }
        result = buildResult.exitStatus;
    }
    destructGlobals();
    return result;
}
