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
#include <charconv>
#include <chrono>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <optional>
#include <span>
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
using std::filesystem::path;
using std::string;
using std::string_view;
using std::vector;

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

    bool acquire(const path &file, string &diagnostic)
    {
#ifdef _WIN32
        const string fileName = file.string();
        handle = CreateFileA(fileName.c_str(), GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            diagnostic = "Could not open the project lock: " + file.string() +
                         "\nWindows error: " + std::to_string(GetLastError());
            return false;
        }
        OVERLAPPED overlapped{};
        if (!LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, MAXDWORD, MAXDWORD,
                        &overlapped))
        {
            diagnostic = "Could not acquire the project lock: " + file.string() +
                         "\nWindows error: " + std::to_string(GetLastError());
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
            return false;
        }
#else
        descriptor = open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0666);
        if (descriptor == -1)
        {
            diagnostic =
                "Could not open the project lock: " + file.string() + "\nSystem error: " + std::strerror(errno);
            return false;
        }
        while (flock(descriptor, LOCK_EX | LOCK_NB) == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            diagnostic = "Could not acquire the project lock: " + file.string() +
                         "\nSystem error: " + std::strerror(errno);
            close(descriptor);
            descriptor = -1;
            return false;
        }
#endif
        return true;
    }

  private:
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
#else
    int descriptor = -1;
#endif
};

bool parsePositiveInteger(const string_view value, uint16_t &result)
{
    if (value.empty())
    {
        return false;
    }
    uint16_t parsed = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size() || parsed == 0)
    {
        return false;
    }
    result = parsed;
    return true;
}

struct Options
{
    std::optional<path> sourceDirectory;
    std::optional<path> buildDirectory;
    std::optional<string> toolchain;
    std::optional<uint16_t> defaultJobs;
    std::optional<uint16_t> jobs;
    vector<string> targets;
    bool configureOnly = false;
    bool reconfigure = false;
    bool recompile = false;
    bool dryRun = false;
    bool headerUnitsOnly = false;
    bool standalone = false;
    bool printHashMap = false;
    bool listToolchains = false;
    bool help = false;
};

bool takeValue(const int argc, char **argv, int &index, const string_view option, string &value, string &diagnostic)
{
    if (index + 1 >= argc)
    {
        diagnostic = "Option requires a value: " + string(option);
        return false;
    }
    value = argv[++index];
    if (value.empty())
    {
        diagnostic = "Option value cannot be empty: " + string(option);
        return false;
    }
    return true;
}

bool parseOptions(const int argc, char **argv, Options &options, string &diagnostic)
{
    bool targetsOnly = false;
    for (int index = 1; index < argc; ++index)
    {
        const string_view argument(argv[index]);
        if (targetsOnly)
        {
            options.targets.emplace_back(argument);
            continue;
        }
        if (argument == "--")
        {
            targetsOnly = true;
            continue;
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
        if (argument == "-S" || argument == "-B" || argument == "--toolchain" ||
            argument == "--default-jobs" || argument == "-j" || argument == "--target")
        {
            string value;
            if (!takeValue(argc, argv, index, argument, value, diagnostic))
            {
                return false;
            }
            if (argument == "-S")
            {
                options.sourceDirectory = path(value);
            }
            else if (argument == "-B")
            {
                options.buildDirectory = path(value);
            }
            else if (argument == "--toolchain")
            {
                options.toolchain = std::move(value);
            }
            else if (argument == "--target")
            {
                options.targets.emplace_back(std::move(value));
            }
            else
            {
                uint16_t parsed = 0;
                if (!parsePositiveInteger(value, parsed))
                {
                    diagnostic =
                        "Expected an integer from 1 through 65535 after " + string(argument) + ": " + value;
                    return false;
                }
                if (argument == "--default-jobs")
                {
                    options.defaultJobs = parsed;
                }
                else
                {
                    options.jobs = parsed;
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
        else if (argument == "--dry-run")
        {
            options.dryRun = true;
        }
        else if (argument == "--header-units-only")
        {
            options.headerUnitsOnly = true;
        }
        else if (argument == "--standalone")
        {
            options.standalone = true;
        }
        else if (argument == "--print-hash-map")
        {
            options.printHashMap = true;
        }
        else if (!argument.empty() && argument.front() == '-')
        {
            diagnostic = "Unknown hbuild option: " + string(argument);
            return false;
        }
        else
        {
            options.targets.emplace_back(argument);
        }
    }

    if (options.listToolchains && (options.buildDirectory || options.toolchain ||
                                   options.defaultJobs || options.jobs || !options.targets.empty() ||
                                   options.configureOnly || options.reconfigure || options.recompile || options.dryRun ||
                                   options.headerUnitsOnly || options.standalone || options.printHashMap))
    {
        diagnostic = "--list-toolchains cannot be combined with a build request.";
        return false;
    }
    return true;
}

void printUsage()
{
    printMessage(
        "Usage: hbuild [options] [targets...] [-- targets...]\n"
        "\n"
        "Project selection:\n"
        "  -S <directory>          Source directory containing hmake.cpp\n"
        "  -B <directory>          Build directory; must be a strict child of the source directory\n"
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
        "  --target <name>         Add a target (repeatable)\n");
}

path normalizePath(const path &base, const path &input, string &diagnostic)
{
    path result = input.is_relative() ? base / input : input;
    if (result.is_relative())
    {
        std::error_code error;
        result = std::filesystem::absolute(result, error);
        if (error)
        {
            diagnostic = "Could not make path absolute: " + input.string() + "\nSystem error: " + error.message();
            return {};
        }
    }
    result = result.lexically_normal();
    if (result != result.root_path() && !result.has_filename())
    {
        result = result.parent_path();
    }
#ifdef _WIN32
    string lowered = result.string();
    lowerCaseOnWindows(lowered.data(), lowered.size());
    result = path(lowered);
#endif
    return result;
}

bool isRegularFile(const path &file)
{
    std::error_code error;
    return std::filesystem::is_regular_file(file, error) && !error;
}

std::optional<path> findParentContaining(const path &start, const string_view filename)
{
    path candidate = start;
    while (true)
    {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate / filename, error) && !error)
        {
            return candidate;
        }
        const path parent = candidate.parent_path();
        if (parent.empty() || parent == candidate)
        {
            break;
        }
        candidate = parent;
    }
    return std::nullopt;
}

std::optional<path> resolveSourceDirectory(const std::optional<path> &requestedDirectory,
                                           const string *cachedDirectory, const path &invocationDirectory,
                                           const path &buildDirectory, string &diagnostic)
{
    path sourceDirectory;
    if (requestedDirectory)
    {
        sourceDirectory = normalizePath(invocationDirectory, *requestedDirectory, diagnostic);
    }
    else if (cachedDirectory != nullptr)
    {
        sourceDirectory = normalizePath(buildDirectory, *cachedDirectory, diagnostic);
    }
    else
    {
        std::optional<path> found = findParentContaining(invocationDirectory, "hmake.cpp");
        if (!found && buildDirectory != invocationDirectory)
        {
            found = findParentContaining(buildDirectory, "hmake.cpp");
        }
        if (!found)
        {
            diagnostic = "Could not find hmake.cpp in the current directory or any parent.\n"
                         "Use -S <directory> to select the project source directory.";
        }
        return found;
    }

    if (!diagnostic.empty())
    {
        return std::nullopt;
    }
    if (!isRegularFile(sourceDirectory / "hmake.cpp"))
    {
        diagnostic = "The source directory must contain hmake.cpp.\nSource directory: " +
                     sourceDirectory.string();
        return std::nullopt;
    }
    return sourceDirectory;
}

struct Command
{
    path executable;
    vector<string> arguments;
    path workingDirectory;
};

#ifdef _WIN32
string quoteWindowsArgument(const string_view argument)
{
    // The result is consumed by cmd.exe and then by the child C runtime. Quote both empty arguments and cmd
    // metacharacters even when no whitespace is present; the backslash handling preserves the final child argv.
    if (!argument.empty() && argument.find_first_of(" \t\r\n\f\v\"&|<>()^%!") == string_view::npos)
    {
        return string(argument);
    }
    string result = "\"";
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
            result.append(backslashes * 2 + 1, '\\');
            result.push_back('"');
            backslashes = 0;
            continue;
        }
        result.append(backslashes, '\\');
        backslashes = 0;
        result.push_back(character);
    }
    result.append(backslashes * 2, '\\');
    result.push_back('"');
    return result;
}

#endif

string renderCommand(const Command &command)
{
    string result;
#ifdef _WIN32
    if (!command.workingDirectory.empty())
    {
        result = "cd /d ";
        result += quoteWindowsArgument(command.workingDirectory.string());
        result += " && ";
    }
    result += quoteWindowsArgument(command.executable.string());
    for (const string &argument : command.arguments)
    {
        result.push_back(' ');
        result += quoteWindowsArgument(argument);
    }
#else
    auto quote = [](const string_view argument) {
        if (!argument.empty() && argument.find_first_of(" \t\r\n\f\v'\"\\$`!&;|<>()[]{}*?#~") == string_view::npos)
        {
            return string(argument);
        }
        string quoted = "'";
        for (const char character : argument)
        {
            if (character == '\'')
            {
                quoted += "'\\''";
            }
            else
            {
                quoted.push_back(character);
            }
        }
        quoted.push_back('\'');
        return quoted;
    };
    if (!command.workingDirectory.empty())
    {
        result = "cd ";
        result += quote(command.workingDirectory.string());
        result += " && ";
    }
    result += quote(command.executable.string());
    for (const string &argument : command.arguments)
    {
        result.push_back(' ');
        result += quote(argument);
    }
#endif
    return result;
}

uint64_t commandHash(const Command &command)
{
    STACK_PMR_STRING(semantic, 16 * 1024)
    semantic.push_back('X');
    semantic += command.executable.string();
    semantic.push_back('\0');
    semantic.push_back('W');
    semantic += command.workingDirectory.string();
    semantic.push_back('\0');
    for (const string &original : command.arguments)
    {
        semantic.push_back('A');
        semantic += original;
        semantic.push_back('\0');
    }
    return rapidhash(semantic.data(), semantic.size());
}

struct BuildCachePrefix
{
    uint64_t ordinaryTailOffset = 0;
    uint64_t ordinaryTailSize = 0;
    bool valid = false;
};

bool loadBuildCachePrefix(const path &file, const uint64_t nodeCount, BuildCachePrefix &prefix, string &diagnostic)
{
    std::error_code error;
    bool exists = std::filesystem::is_regular_file(file, error);
    if (error == std::errc::no_such_file_or_directory)
    {
        error.clear();
        exists = false;
    }
    if (error)
    {
        diagnostic = "Could not inspect build cache: " + file.string() + "\nSystem error: " + error.message();
        return false;
    }
    if (!exists)
    {
        return true;
    }

    const uint64_t fileSize = std::filesystem::file_size(file, error);
    if (error)
    {
        diagnostic = "Could not determine the build cache size: " + file.string() +
                     "\nSystem error: " + error.message();
        return false;
    }
    const string fileName = file.string();
    FILE *input = std::fopen(fileName.c_str(), "rb");
    if (input == nullptr)
    {
        diagnostic = "Could not open the build cache: " + fileName +
                     "\nSystem error: " + std::strerror(errno);
        return false;
    }

    uint64_t offset = 0;
    bool inputError = false;
    auto readBytes = [&](void *const destination, const uint64_t size) {
        if (offset > fileSize || size > fileSize - offset)
        {
            return false;
        }
        errno = 0;
        if (size != 0 && std::fread(destination, 1, size, input) != size)
        {
            if (std::ferror(input))
            {
                const int readError = errno;
                diagnostic = "Could not read the build cache: " + fileName +
                             "\nSystem error: " +
                             (readError == 0 ? string("I/O error") : std::strerror(readError));
                inputError = true;
            }
            return false;
        }
        offset += size;
        return true;
    };

    uint64_t fixedPrefix[5];
    bool parsed = readBytes(fixedPrefix, sizeof(fixedPrefix));
    auto readIds = [&](flat_hash_set<Node *> &nodes) {
        uint32_t count = 0;
        if (!readBytes(&count, sizeof(count)) || count > nodeCount)
        {
            return false;
        }
        const uint64_t idsSize = static_cast<uint64_t>(count) * sizeof(uint32_t);
        STACK_PMR_VECTOR(uint32_t, ids, 64)
        ids.reserve(count);
        ids.resize(count);
        if (!readBytes(ids.data(), idsSize))
        {
            return false;
        }
        nodes.reserve(count);
        for (const uint32_t id : ids)
        {
            if (id >= nodeCount)
            {
                return false;
            }
            Node *node = Node::getHalfNode(id);
            node->doStatFile = true;
            node->doHashFile = true;
            nodes.emplace(node);
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
        return !inputError;
    }

    buildCacheContentHash = fixedPrefix[0];
    buildExeCommandHash = fixedPrefix[1];
    configureExeCommandHash = fixedPrefix[2];
    selectedToolchainCommandCache = fixedPrefix[3];
    projectCacheContentCache = fixedPrefix[4];
    prefix.ordinaryTailOffset = offset;
    prefix.ordinaryTailSize = fileSize - offset;
    prefix.valid = true;
    return true;
}

bool writeBuildCachePrefix(const path &file, BuildCachePrefix &prefix, const bool preserveOrdinaryTail,
                           const bool changed, string &diagnostic)
{
    if (!changed && (preserveOrdinaryTail || prefix.ordinaryTailSize == 0))
    {
        return true;
    }

    const uint64_t prefixSize = 4 * sizeof(uint64_t) + 2 * sizeof(uint32_t) +
                                (recompileNodes.size() + reconfigureNodes.size()) * sizeof(uint32_t);
    const uint64_t tailSize = preserveOrdinaryTail ? prefix.ordinaryTailSize : 0;
    const uint64_t totalSize = prefixSize + tailSize;
    FILE *tailInput = nullptr;
    if (tailSize != 0)
    {
        const string fileName = file.string();
        tailInput = std::fopen(fileName.c_str(), "rb");
        if (tailInput == nullptr)
        {
            diagnostic = "Could not open the build cache to preserve its target rows: " + fileName +
                         "\nSystem error: " + std::strerror(errno);
            return false;
        }
        errno = 0;
#ifdef _WIN32
        const int seekResult = _fseeki64(tailInput, static_cast<int64_t>(prefix.ordinaryTailOffset), SEEK_SET);
#else
        const int seekResult = fseeko(tailInput, static_cast<off_t>(prefix.ordinaryTailOffset), SEEK_SET);
#endif
        if (seekResult != 0)
        {
            const int seekError = errno;
            const string errorMessage = seekError == 0 ? "I/O error" : std::strerror(seekError);
            std::fclose(tailInput);
            diagnostic = "Could not seek to the build-cache target rows: " + fileName +
                         "\nSystem error: " + errorMessage;
            return false;
        }
    }

    uint64_t tailBytesRead = 0;
    int tailReadError = 0;
    string fileBuffer;
    fileBuffer.resize_and_overwrite(totalSize, [&](char *bytes, const uint64_t bufferSize) {
        uint64_t offset = 0;
        auto writeUint32 = [&](const uint32_t value) {
            std::memcpy(bytes + offset, &value, sizeof(value));
            offset += sizeof(value);
        };
        const auto writeUint64 = [&](const uint64_t value) {
            std::memcpy(bytes + offset, &value, sizeof(value));
            offset += sizeof(value);
        };
        writeUint64(buildExeCommandHash);
        writeUint64(configureExeCommandHash);
        writeUint64(selectedToolchainCommandCache);
        writeUint64(projectCacheContentCache);
        writeUint32(static_cast<uint32_t>(recompileNodes.size()));
        for (const Node *node : recompileNodes)
        {
            assert(node != nullptr && node->myId < nodeIndices.size() && nodeIndices[node->myId] == node);
            writeUint32(node->myId);
        }
        writeUint32(static_cast<uint32_t>(reconfigureNodes.size()));
        for (const Node *node : reconfigureNodes)
        {
            assert(node != nullptr && node->myId < nodeIndices.size() && nodeIndices[node->myId] == node);
            writeUint32(node->myId);
        }
        if (tailInput != nullptr)
        {
            errno = 0;
            tailBytesRead = std::fread(bytes + offset, 1, tailSize, tailInput);
            tailReadError = errno;
            offset += tailBytesRead;
        }
        if (tailBytesRead == tailSize)
        {
            assert(offset == bufferSize);
        }
        return offset;
    });

    if (tailInput != nullptr)
    {
        const bool readFailed = tailBytesRead != tailSize;
        const bool streamError = std::ferror(tailInput);
        std::fclose(tailInput);
        if (readFailed)
        {
            diagnostic = "Could not read the complete build-cache target rows: " + file.string() +
                         "\nExpected bytes: " + std::to_string(tailSize) +
                         "\nRead bytes: " + std::to_string(tailBytesRead);
            if (streamError)
            {
                diagnostic += "\nSystem error: " +
                              (tailReadError == 0 ? string("I/O error") : std::strerror(tailReadError));
            }
            return false;
        }
    }
    assert(fileBuffer.size() == totalSize);
    writeCacheFile(file.string(), fileBuffer, &buildCacheContentHash);
    prefix.ordinaryTailOffset = sizeof(buildCacheContentHash) + prefixSize;
    prefix.ordinaryTailSize = tailSize;
    prefix.valid = true;
    return true;
}

bool invalidationSetChanged(const flat_hash_set<Node *> &nodes, const std::span<const uint64_t> cachedSnapshots)
{
    assert(nodes.size() * 2 == cachedSnapshots.size());
    uint64_t index = 0;
    for (const Node *node : nodes)
    {
        const uint64_t cachedLastWriteTime = cachedSnapshots[index++];
        const uint64_t cachedContentHash = cachedSnapshots[index++];
        if (node->lastWriteTime != cachedLastWriteTime || node->contentHash != cachedContentHash)
        {
            return true;
        }
    }
    return false;
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

const Toolchain *resolveToolchain(const string_view requested, string &diagnostic)
{
    const string_view selected = requested.empty() ? toolchains.defaultName() : requested;
    if (selected.empty())
    {
        diagnostic = "No toolchain is registered.";
        return nullptr;
    }
    const Toolchain *resolved = toolchains.find(selected);
    if (resolved == nullptr)
    {
        diagnostic = "Unknown toolchain: " + string(selected) + "\nUse --list-toolchains to list valid names.";
        return nullptr;
    }
    return resolved;
}

Command makeCompileCommand(const Toolchain &toolchain, const bool configureMode, const path &sourceFile,
                           const path &outputFile, const path &dependencyFile, const path &objectFile,
                           const path &workingDirectory)
{
    Command command;
    command.executable = toolchain.compiler.bTPath;
    command.workingDirectory = workingDirectory;
    command.arguments.reserve(toolchain.bootstrapArguments.size() + 32);
    command.arguments.insert(command.arguments.end(), toolchain.bootstrapArguments.begin(),
                             toolchain.bootstrapArguments.end());
    const path staticLibrary = configureMode ? path(HCONFIGURE_C_STATIC_LIB_PATH) : path(HCONFIGURE_B_STATIC_LIB_PATH);
    constexpr string_view includePaths[] = {HCONFIGURE_HEADER, THIRD_PARTY_HEADER, RAPIDJSON_HEADER};

    if (toolchain.style == "gnu")
    {
        constexpr string_view common[] = {"-std=c++23",          "-O0",
                                          "-fno-exceptions",     "-fno-rtti",
                                          "-fvisibility=hidden", "-ffunction-sections",
                                          "-fdata-sections",     "-pthread",
                                          "-nostdinc",           "-nostdinc++"};
        for (const string_view argument : common)
        {
            command.arguments.emplace_back(argument);
        }
        if (!configureMode)
        {
            command.arguments.emplace_back("-DBUILD_MODE");
            command.arguments.emplace_back("-DNDEBUG");
        }
        for (const string_view include : includePaths)
        {
            command.arguments.emplace_back("-I");
            command.arguments.emplace_back(include);
        }
        for (const string &include : toolchain.includeDirs)
        {
            command.arguments.emplace_back("-isystem");
            command.arguments.emplace_back(include);
        }
        command.arguments.emplace_back("-MMD");
        command.arguments.emplace_back("-MF");
        command.arguments.emplace_back(dependencyFile.string());
        command.arguments.emplace_back("-MT");
        command.arguments.emplace_back(outputFile.string());
        command.arguments.emplace_back(sourceFile.string());
        for (const string &directory : toolchain.libraryDirs)
        {
            command.arguments.emplace_back("-L");
            command.arguments.emplace_back(directory);
        }
        command.arguments.emplace_back("-Wl,--gc-sections");
        command.arguments.emplace_back("-Wl,--whole-archive");
        command.arguments.emplace_back(staticLibrary.string());
        command.arguments.emplace_back("-Wl,--no-whole-archive");
        command.arguments.emplace_back("-o");
        command.arguments.emplace_back(outputFile.string());
    }
    else
    {
        command.arguments.emplace_back("/std:c++latest");
        command.arguments.emplace_back("/O0");
        command.arguments.emplace_back("/GR-");
        command.arguments.emplace_back("/EHs-c-");
        command.arguments.emplace_back("/D_HAS_EXCEPTIONS=0");
        command.arguments.emplace_back("/MT");
        command.arguments.emplace_back("/nologo");
        command.arguments.emplace_back("/X");
        if (!configureMode)
        {
            command.arguments.emplace_back("/DBUILD_MODE");
            command.arguments.emplace_back("/DNDEBUG");
        }
        for (const string_view include : includePaths)
        {
            command.arguments.emplace_back("/I" + string(include));
        }
        for (const string &include : toolchain.includeDirs)
        {
            command.arguments.emplace_back("/I" + include);
        }
        command.arguments.emplace_back("/sourceDependencies");
        command.arguments.emplace_back(dependencyFile.string());
        command.arguments.emplace_back(sourceFile.string());
        command.arguments.emplace_back("/Fo" + objectFile.string());
        command.arguments.emplace_back("/link");
        command.arguments.emplace_back("/SUBSYSTEM:CONSOLE");
        command.arguments.emplace_back("/NOLOGO");
        for (const string &directory : toolchain.libraryDirs)
        {
            command.arguments.emplace_back("/LIBPATH:" + directory);
        }
        command.arguments.emplace_back("/WHOLEARCHIVE:" + staticLibrary.string());
        command.arguments.emplace_back("kernel32.lib");
        command.arguments.emplace_back("synchronization.lib");
        command.arguments.emplace_back("user32.lib");
        command.arguments.emplace_back("gdi32.lib");
        command.arguments.emplace_back("winspool.lib");
        command.arguments.emplace_back("shell32.lib");
        command.arguments.emplace_back("ole32.lib");
        command.arguments.emplace_back("oleaut32.lib");
        command.arguments.emplace_back("uuid.lib");
        command.arguments.emplace_back("comdlg32.lib");
        command.arguments.emplace_back("advapi32.lib");
        command.arguments.emplace_back("/OUT:" + outputFile.string());
    }
    return command;
}

struct CompileTask
{
    string label;
    path finalExecutable;
    // Installed atomically only after both bootstrap compilations and their dependency parsing succeed.
    path temporaryExecutable;
    path temporaryDependency;
    path temporaryObject;
    Command command;
    uint64_t semanticHash = 0;
    RunCommand::OutputAndStatus result;
    double elapsedSeconds = 0;
};

CompileTask makeCompileTask(const Toolchain &toolchain, const bool configureMode, const path &sourceFile,
                            const path &buildDirectory)
{
    CompileTask task;
    task.label = configureMode ? "configure" : "build";
    task.finalExecutable = buildDirectory / getActualNameFromTargetName(TargetType::EXECUTABLE, os, task.label);
    task.temporaryExecutable = task.finalExecutable.string() + ".tmp";
    const path dependencyDirectory = buildDirectory / ".hbuild";
    task.temporaryDependency =
        dependencyDirectory / (task.label + (toolchain.style == "msvc" ? ".json.tmp" : ".d.tmp"));
    task.temporaryObject = dependencyDirectory / (task.label + ".obj.tmp");
    task.command = makeCompileCommand(toolchain, configureMode, sourceFile, task.temporaryExecutable,
                                      task.temporaryDependency, task.temporaryObject, buildDirectory);
    task.semanticHash = commandHash(task.command);
    return task;
}

void removeTemporaryTaskFiles(const CompileTask &task)
{
    std::error_code ignored;
    std::filesystem::remove(task.temporaryExecutable, ignored);
    ignored.clear();
    std::filesystem::remove(task.temporaryDependency, ignored);
    ignored.clear();
    std::filesystem::remove(task.temporaryObject, ignored);
}

bool compileBootstrapExecutables(CompileTask &configureTask, CompileTask &buildTask, const Compiler &compiler,
                                 const Node *compiledSource, flat_hash_set<Node *> &dependencies, string &diagnostic)
{
    removeTemporaryTaskFiles(configureTask);
    removeTemporaryTaskFiles(buildTask);

    const auto execute = [](CompileTask &task) {
        const auto started = std::chrono::steady_clock::now();
        const string command = renderCommand(task.command);
        task.result = RunCommand::runProcess(command);
        task.elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    };
    execute(configureTask);
    if (configureTask.result.exitStatus == EXIT_SUCCESS)
    {
        execute(buildTask);
    }

    auto reportCompileFailure = [&](const CompileTask &task) {
        diagnostic = "Could not compile the generated " + task.label + " executable.\nExit code: " +
                     std::to_string(task.result.exitStatus) + "\nCommand: " + renderCommand(task.command) +
                     "\nCompiler output:\n" + task.result.output;
    };
    if (configureTask.result.exitStatus != 0)
    {
        reportCompileFailure(configureTask);
    }
    else if (buildTask.result.exitStatus != 0)
    {
        reportCompileFailure(buildTask);
    }
    if (!diagnostic.empty())
    {
        removeTemporaryTaskFiles(configureTask);
        removeTemporaryTaskFiles(buildTask);
        return false;
    }

    if (!configureTask.result.output.empty())
    {
        printMessage(configureTask.result.output);
    }
    if (!buildTask.result.output.empty())
    {
        printMessage(buildTask.result.output);
    }
    printMessage(FORMAT("configure compilation time: {:.3f} seconds\n", configureTask.elapsedSeconds));
    printMessage(FORMAT("build compilation time: {:.3f} seconds\n", buildTask.elapsedSeconds));

    if (!isRegularFile(configureTask.temporaryDependency) || !isRegularFile(buildTask.temporaryDependency))
    {
        removeTemporaryTaskFiles(configureTask);
        removeTemporaryTaskFiles(buildTask);
        diagnostic = "Bootstrap compilation did not produce dependency files.";
        return false;
    }

    const auto parse = [&](CompileTask &task) {
        flat_hash_set<Node *> parsed = parseHeaderDeps(
            task.result.output, compiler, task.result.exitStatus, task.temporaryDependency.string(),
            task.command.workingDirectory.string(), compiledSource, false);
        dependencies.insert(parsed.begin(), parsed.end());
    };
    parse(configureTask);
    parse(buildTask);

    replaceFileAtomically(configureTask.temporaryExecutable.string(), configureTask.finalExecutable.string());
    replaceFileAtomically(buildTask.temporaryExecutable.string(), buildTask.finalExecutable.string());
    removeTemporaryTaskFiles(configureTask);
    removeTemporaryTaskFiles(buildTask);
    return true;
}

bool isDirectory(const path &directory)
{
    std::error_code error;
    return std::filesystem::is_directory(directory, error) && !error;
}

struct ProjectContext
{
    Node *hmakeFile = nullptr;
    bool projectCacheExisted = false;
    bool nodesCacheExisted = false;
};

bool resolveProject(const Options &options, const path &invocationDirectory, ProjectContext &context,
                    ProjectLock &projectLock, string &diagnostic)
{
    std::error_code error;
    path buildDirectory;
    if (options.buildDirectory)
    {
        buildDirectory = normalizePath(invocationDirectory, *options.buildDirectory, diagnostic);
    }
    else if (const std::optional<path> cachedBuild = findParentContaining(invocationDirectory, projectCacheFileName))
    {
        buildDirectory = *cachedBuild;
    }
    else
    {
        buildDirectory = invocationDirectory;
    }
    if (!diagnostic.empty())
    {
        return false;
    }

    const path cacheFile = buildDirectory / projectCacheFileName;
    string cacheContents;
    context.projectCacheExisted = isRegularFile(cacheFile);
    if (context.projectCacheExisted)
    {
        cacheContents = fileToString(cacheFile.string());
        string cacheError;
        if (!projectCache.parse(cacheContents, cacheError))
        {
            diagnostic = "Invalid project cache.\nFile: " + cacheFile.string() + "\n" + cacheError;
            return false;
        }
    }

    path sourceDirectory;
    path hmakeFile;
    const auto resolveAndValidateSourceDirectory = [&]() {
        const string *cachedSourceDirectory =
            context.projectCacheExisted ? &projectCache.sourceDirectoryPath : nullptr;
        std::optional<path> resolvedSourceDirectory =
            resolveSourceDirectory(options.sourceDirectory, cachedSourceDirectory, invocationDirectory,
                                   buildDirectory, diagnostic);
        if (!resolvedSourceDirectory)
        {
            return false;
        }
        sourceDirectory = std::move(*resolvedSourceDirectory);
        hmakeFile = sourceDirectory / "hmake.cpp";
        if (!isPathInDirectory(buildDirectory.string(), sourceDirectory.string()))
        {
            diagnostic = "The build directory must be a strict lexical child of the source directory.\n"
                         "Source directory: " +
                         sourceDirectory.string() + "\nBuild directory: " + buildDirectory.string() +
                         "\nHint: from the source directory, run hbuild -B build.";
            return false;
        }
        return true;
    };
    if (!resolveAndValidateSourceDirectory())
    {
        return false;
    }

    const bool buildDirectoryCreated = std::filesystem::create_directories(buildDirectory, error);
    if (error || (!buildDirectoryCreated && !isDirectory(buildDirectory)))
    {
        diagnostic = "Could not create build directory: " + buildDirectory.string();
        if (error)
        {
            diagnostic += "\nSystem error: " + error.message();
        }
        return false;
    }

    const path bootstrapDirectory = buildDirectory / ".hbuild";
    error.clear();
    const bool bootstrapDirectoryCreated = std::filesystem::create_directories(bootstrapDirectory, error);
    if (error || (!bootstrapDirectoryCreated && !isDirectory(bootstrapDirectory)))
    {
        diagnostic = "Could not create hbuild metadata directory: " + bootstrapDirectory.string();
        if (error)
        {
            diagnostic += "\nSystem error: " + error.message();
        }
        return false;
    }
    if (!projectLock.acquire(bootstrapDirectory / "lock", diagnostic))
    {
        return false;
    }

    const bool lockedCacheExisted = isRegularFile(cacheFile);
    const string lockedCacheContents = lockedCacheExisted ? fileToString(cacheFile.string()) : string{};
    if (lockedCacheExisted != context.projectCacheExisted || lockedCacheContents != cacheContents)
    {
        ProjectCache lockedProjectCache;
        if (lockedCacheExisted)
        {
            string cacheError;
            if (!lockedProjectCache.parse(lockedCacheContents, cacheError))
            {
                diagnostic = "Invalid project cache.\nFile: " + cacheFile.string() + "\n" + cacheError;
                return false;
            }
        }
        projectCache = std::move(lockedProjectCache);
        context.projectCacheExisted = lockedCacheExisted;
        if (!resolveAndValidateSourceDirectory())
        {
            return false;
        }
    }

    if (!context.projectCacheExisted || options.sourceDirectory)
    {
        const string selectedSource = sourceDirectory.string();
        projectCache.needsWrite = projectCache.needsWrite || projectCache.sourceDirectoryPath != selectedSource;
        projectCache.sourceDirectoryPath = selectedSource;
    }
    if (options.toolchain)
    {
        projectCache.needsWrite = projectCache.needsWrite || projectCache.toolchainName != *options.toolchain;
        projectCache.toolchainName = *options.toolchain;
    }
    if (options.defaultJobs)
    {
        projectCache.needsWrite = projectCache.needsWrite || projectCache.defaultJobs != *options.defaultJobs;
        projectCache.defaultJobs = *options.defaultJobs;
    }

    toolchains.initialize(sourceDirectory);
    const path nodesFile = buildDirectory / nodesCacheFileName;
    context.nodesCacheExisted = isRegularFile(nodesFile);
    if (context.nodesCacheExisted)
    {
        loadNodesCache(nodesFile);
    }

    currentNode = Node::getHalfNode(invocationDirectory.string());
    srcNode = Node::getHalfNode(sourceDirectory.string());
    configureNode = Node::getHalfNode(buildDirectory.string());
    context.hmakeFile = Node::getNode(hmakeFile.string(), true);
    normalizationBasePath = srcNode->filePath;
    return true;
}

bool removeMetadataFile(const path &file, string &diagnostic)
{
    std::error_code error;
    std::filesystem::remove(file, error);
    if (error)
    {
        diagnostic =
            "Could not invalidate stale metadata file: " + file.string() + "\nSystem error: " + error.message();
        return false;
    }
    return true;
}

bool runGeneratedConfigure(const path &executable, const path &buildDirectory, string &diagnostic)
{
    Command command{executable, {}, buildDirectory};
    printMessage("Running configure\n");
    const auto started = std::chrono::steady_clock::now();
    const string rendered = renderCommand(command);
    const RunCommand::OutputAndStatus result = RunCommand::runProcess(rendered);
    const double elapsedSeconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    if (!result.output.empty())
    {
        printMessage(result.output);
    }
    printMessage(FORMAT("configure execution time: {:.3f} seconds\n", elapsedSeconds));
    if (result.exitStatus != 0)
    {
        diagnostic =
            "Generated configure executable failed with exit code " + std::to_string(result.exitStatus) + ".";
        return false;
    }
    return true;
}

int runGeneratedBuild(const Options &options, const path &executable, const path &workingDirectory)
{
    Command command;
    command.executable = executable;
    command.workingDirectory = workingDirectory;
    if (options.dryRun)
    {
        command.arguments.emplace_back("--dry-run");
    }
    if (options.headerUnitsOnly)
    {
        command.arguments.emplace_back("--header-units-only");
    }
    if (options.standalone)
    {
        command.arguments.emplace_back("--standalone");
    }
    if (options.printHashMap)
    {
        command.arguments.emplace_back("--print-hash-map");
    }
    if (options.jobs)
    {
        command.arguments.emplace_back("--jobs");
        command.arguments.emplace_back(std::to_string(*options.jobs));
    }
    if (!options.targets.empty())
    {
        command.arguments.emplace_back("--");
        command.arguments.insert(command.arguments.end(), options.targets.begin(), options.targets.end());
    }
    const string rendered = renderCommand(command);
    const RunCommand::OutputAndStatus result = RunCommand::runProcess(rendered);
    if (!result.output.empty())
    {
        printMessage(result.output);
    }
    return result.exitStatus;
}

} // namespace

int runBootstrap(const int argc, char **argv)
{
    std::error_code invocationError;
    path invocationDirectory = std::filesystem::current_path(invocationError);
    if (invocationError)
    {
        printErrorMessage("Could not determine the current directory.\nSystem error: " + invocationError.message());
    }
#ifdef _WIN32
    string loweredInvocationDirectory = invocationDirectory.string();
    lowerCaseOnWindows(loweredInvocationDirectory.data(), loweredInvocationDirectory.size());
    invocationDirectory = path(loweredInvocationDirectory);
#endif

    Options options;
    string diagnostic;
    if (!parseOptions(argc, argv, options, diagnostic))
    {
        printErrorMessage(diagnostic);
    }
    if (options.help)
    {
        printUsage();
        return 0;
    }
    if (options.listToolchains)
    {
        const std::optional<path> sourceDirectory =
            resolveSourceDirectory(options.sourceDirectory, nullptr, invocationDirectory, invocationDirectory,
                                   diagnostic);
        if (!sourceDirectory)
        {
            printErrorMessage(diagnostic);
        }
        toolchains.initialize(*sourceDirectory);
        printMessage(toolchains.toJson());
        printMessage("\n");
        return 0;
    }

    constructGlobals();
    const int result = [&]() -> int {
        ProjectContext project;
        ProjectLock projectLock;
        if (!resolveProject(options, invocationDirectory, project, projectLock, diagnostic))
        {
            printErrorMessage(diagnostic);
        }

        const Toolchain *projectToolchain = resolveToolchain(projectCache.toolchainName, diagnostic);
        const Toolchain *bootstrapToolchain = resolveToolchain({}, diagnostic);
        if (projectToolchain == nullptr || bootstrapToolchain == nullptr)
        {
            printErrorMessage(diagnostic);
        }
        projectCache.needsWrite = projectCache.needsWrite || projectCache.toolchainName != projectToolchain->name;
        projectCache.toolchainName = projectToolchain->name;
        if (projectCache.needsWrite)
        {
            STACK_PMR_STRING(cacheContents, 4 * 1024)
            string cacheError;
            if (!projectCache.serialize(cacheContents, cacheError))
            {
                printErrorMessage(cacheError);
            }
            writeCacheFile((path(configureNode->filePath) / projectCacheFileName).string(), cacheContents);
            projectCache.needsWrite = false;
        }

        CompileTask configureTask = makeCompileTask(*bootstrapToolchain, true, project.hmakeFile->filePath,
                                                    configureNode->filePath);
        CompileTask buildTask = makeCompileTask(*bootstrapToolchain, false, project.hmakeFile->filePath,
                                                configureNode->filePath);

        const path nodesFile = path(configureNode->filePath) / nodesCacheFileName;
        const path configFile = path(configureNode->filePath) / configCacheFileName;
        const path buildCacheFile = path(configureNode->filePath) / buildCacheFileName;
        BuildCachePrefix prefix;
        if (project.nodesCacheExisted &&
            !loadBuildCachePrefix(buildCacheFile, nodeIndices.size(), prefix, diagnostic))
        {
            printErrorMessage(diagnostic);
        }

        const bool nodesMetadataMissing = !project.nodesCacheExisted;
        const bool buildMetadataMissing = !prefix.valid;
        bool prefixChanged = buildMetadataMissing;
        const bool configExistsInitially = isRegularFile(configFile);
        const uint64_t selectedToolchainCache = toolchainCommandCache(*projectToolchain);
        const uint64_t projectContentCache = projectCache.contentCache();
        bool mustCompile = options.recompile || !isRegularFile(configureTask.finalExecutable) ||
                           !isRegularFile(buildTask.finalExecutable) || nodesMetadataMissing || buildMetadataMissing;
        bool mustConfigure = options.reconfigure || mustCompile || !configExistsInitially;

        if (prefix.valid)
        {
            STACK_PMR_VECTOR(uint64_t, cachedRecompileSnapshots, 64)
            cachedRecompileSnapshots.reserve(recompileNodes.size() * 2);
            for (const Node *node : recompileNodes)
            {
                cachedRecompileSnapshots.emplace_back(node->lastWriteTime);
                cachedRecompileSnapshots.emplace_back(node->contentHash);
            }
            STACK_PMR_VECTOR(uint64_t, cachedReconfigureSnapshots, 64)
            cachedReconfigureSnapshots.reserve(reconfigureNodes.size() * 2);
            for (const Node *node : reconfigureNodes)
            {
                cachedReconfigureSnapshots.emplace_back(node->lastWriteTime);
                cachedReconfigureSnapshots.emplace_back(node->contentHash);
            }
            Builder::checkNodes();
            const bool recompileChanged = invalidationSetChanged(recompileNodes, cachedRecompileSnapshots);
            const bool reconfigureChanged = invalidationSetChanged(reconfigureNodes, cachedReconfigureSnapshots);
            const bool commandCacheChanged =
                buildExeCommandHash != buildTask.semanticHash || configureExeCommandHash != configureTask.semanticHash;
            const bool projectInputsChanged =
                selectedToolchainCommandCache != selectedToolchainCache ||
                projectCacheContentCache != projectContentCache;
            mustCompile = mustCompile || commandCacheChanged || recompileChanged;
            mustConfigure = mustConfigure || commandCacheChanged || recompileChanged || projectInputsChanged ||
                            reconfigureChanged;
        }
        else
        {
            recompileNodes.clear();
            reconfigureNodes.clear();
        }

        if (nodesMetadataMissing || buildMetadataMissing)
        {
            prefix = {};
        }
        if (nodesMetadataMissing || buildMetadataMissing || options.reconfigure)
        {
            if (!removeMetadataFile(configFile, diagnostic))
            {
                printErrorMessage(diagnostic);
            }
        }

        if (mustCompile)
        {
            prefixChanged = true;
            printMessage("Compiling configure and build executables\n");
            flat_hash_set<Node *> dependencies;
            if (!compileBootstrapExecutables(configureTask, buildTask, bootstrapToolchain->compiler,
                                             project.hmakeFile, dependencies, diagnostic))
            {
                printErrorMessage(diagnostic);
            }
            dependencies.emplace(project.hmakeFile);
            dependencies.emplace(Node::getNodeNonNormalized(HCONFIGURE_C_STATIC_LIB_PATH, true));
            dependencies.emplace(Node::getNodeNonNormalized(HCONFIGURE_B_STATIC_LIB_PATH, true));
            if (isRegularFile(bootstrapToolchain->compiler.bTPath))
            {
                dependencies.emplace(Node::getNodeNonNormalized(bootstrapToolchain->compiler.bTPath, true));
            }
            recompileNodes = std::move(dependencies);
        }

        prefixChanged = prefixChanged || buildExeCommandHash != buildTask.semanticHash ||
                        configureExeCommandHash != configureTask.semanticHash ||
                        selectedToolchainCommandCache != selectedToolchainCache ||
                        projectCacheContentCache != projectContentCache;
        buildExeCommandHash = buildTask.semanticHash;
        configureExeCommandHash = configureTask.semanticHash;
        selectedToolchainCommandCache = selectedToolchainCache;
        projectCacheContentCache = projectContentCache;

        // Configuration reconstructs this user-owned set directly from the current hmake.cpp. Clear the previous
        // snapshot only after hbuild has used it for the take-off decision, so removed entries do not live forever.
        if (mustConfigure)
        {
            prefixChanged = prefixChanged || !reconfigureNodes.empty();
            reconfigureNodes.clear();
        }

        if (mustCompile)
        {
            for (Node *node : recompileNodes)
            {
                node->doStatFile = true;
                node->doHashFile = true;
            }
            Builder::checkNodes();
        }
        writeNodesCache();
        const bool preserveOrdinaryTail =
            configExistsInitially && !options.reconfigure && !nodesMetadataMissing && prefix.valid;
        if (!writeBuildCachePrefix(buildCacheFile, prefix, preserveOrdinaryTail, prefixChanged, diagnostic))
        {
            printErrorMessage(diagnostic);
        }

        if (mustConfigure)
        {
            if (!runGeneratedConfigure(configureTask.finalExecutable, configureNode->filePath, diagnostic))
            {
                string removeDiagnostic;
                removeMetadataFile(configFile, removeDiagnostic);
                if (!removeDiagnostic.empty())
                {
                    diagnostic += "\nAdditionally, " + removeDiagnostic;
                }
                printErrorMessage(diagnostic);
            }
            if (!isRegularFile(configFile) || !isRegularFile(nodesFile) || !isRegularFile(buildCacheFile))
            {
                printErrorMessage("Generated configure completed without producing all three cache artifacts.");
            }

        }

        if (options.configureOnly)
        {
            return 0;
        }

        const string_view buildDirectory = configureNode->filePath;
        const string_view invocationPath = currentNode->filePath;
        const bool invocationIsInBuild =
            invocationPath == buildDirectory || isPathInDirectory(invocationPath, buildDirectory);
        const path buildWorkingDirectory = invocationIsInBuild ? path(invocationPath) : path(buildDirectory);
        return runGeneratedBuild(options, buildTask.finalExecutable, buildWorkingDirectory);
    }();
    destructGlobals();
    return result;
}
