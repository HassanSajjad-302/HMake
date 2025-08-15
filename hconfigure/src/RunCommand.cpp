
#ifdef USE_HEADER_UNITS
import "RunCommand.hpp";
import "BuildSystemFunctions.hpp";
import "CppSourceTarget.hpp";
import "TargetCacheDiskWriteManager.hpp";
import "subprocess/subprocess.h";
import "Utilities.hpp";
#else
#include "RunCommand.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include "TargetCacheDiskWriteManager.hpp"
#include "Utilities.hpp"
#endif
#include <Configuration.hpp>

#ifdef WIN32
#include <Windows.h>

// Copied From Ninja code-base.
/// Wraps a synchronous execution of a CL subprocess.
struct CLWrapper
{
    CLWrapper() : env_block_(nullptr)
    {
    }

    /// Set the environment block (as suitable for CreateProcess) to be used
    /// by Run().
    void SetEnvBlock(void *env_block)
    {
        env_block_ = env_block;
    }

    /// Start a process and gather its raw output.  Returns its exit code.
    /// Crashes (calls Fatal()) on error.
    int Run(const std::string &command, std::string *output) const;

    void *env_block_;
};

// TODO
//  Error should throw and not exit
void Fatal(const char *msg, ...)
{
    va_list ap;
    fprintf(stderr, "ninja: fatal: ");
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, "\n");
#ifdef _WIN32
    // On Windows, some tools may inject extra threads.
    // exit() may block on locks held by those threads, so forcibly exit.
    fflush(stderr);
    fflush(stdout);
    ExitProcess(1);
#else
    exit(1);
#endif
}
string GetLastErrorString()
{
    DWORD err = GetLastError();

    char *msg_buf;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                   err, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), (char *)&msg_buf, 0, nullptr);

    if (msg_buf == nullptr)
    {
        char fallback_msg[128] = {0};
        snprintf(fallback_msg, sizeof(fallback_msg), "GetLastError() = %d", err);
        return fallback_msg;
    }

    string msg = msg_buf;
    LocalFree(msg_buf);
    return msg;
}

void Win32Fatal(const char *function, const char *hint = nullptr)
{
    if (hint)
    {
        Fatal("%s: %s (%s)", function, GetLastErrorString().c_str(), hint);
    }
    else
    {
        Fatal("%s: %s", function, GetLastErrorString().c_str());
    }
}

int CLWrapper::Run(const string &command, string *output) const
{
    SECURITY_ATTRIBUTES security_attributes = {};
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle = TRUE;

    HANDLE stdout_read, stdout_write;
    if (!CreatePipe(&stdout_read, &stdout_write, &security_attributes, 0))
        Win32Fatal("CreatePipe");

    if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0))
        Win32Fatal("SetHandleInformation");

    PROCESS_INFORMATION process_info = {};
    STARTUPINFOA startup_info = {};
    startup_info.cb = sizeof(STARTUPINFOA);
    startup_info.hStdError = stdout_write;
    startup_info.hStdOutput = stdout_write;
    startup_info.dwFlags |= STARTF_USESTDHANDLES;

    if (!CreateProcessA(nullptr, (char *)command.c_str(), nullptr, nullptr,
                        /* inherit handles */ TRUE, 0, env_block_, nullptr, &startup_info, &process_info))
    {
        Win32Fatal("CreateProcess");
    }

    if (!CloseHandle(stdout_write))
    {
        Win32Fatal("CloseHandle");
    }

    // Read all output of the subprocess.
    DWORD read_len = 1;
    while (read_len)
    {
        char buf[64 << 10];
        read_len = 0;
        if (!::ReadFile(stdout_read, buf, sizeof(buf), &read_len, nullptr) && GetLastError() != ERROR_BROKEN_PIPE)
        {
            Win32Fatal("ReadFile");
        }
        output->append(buf, read_len);
    }

    // Wait for it to exit and grab its exit code.
    if (WaitForSingleObject(process_info.hProcess, INFINITE) == WAIT_FAILED)
        Win32Fatal("WaitForSingleObject");
    DWORD exit_code = 0;
    if (!GetExitCodeProcess(process_info.hProcess, &exit_code))
        Win32Fatal("GetExitCodeProcess");

    if (!CloseHandle(stdout_read) || !CloseHandle(process_info.hProcess) || !CloseHandle(process_info.hThread))
    {
        Win32Fatal("CloseHandle");
    }

    return exit_code;
}

#else

#include <sys/wait.h>
#include <unistd.h>
struct CLWrapper
{
    CLWrapper() : env_block_(nullptr)
    {
    }

    /// Set the environment block (as suitable for CreateProcess) to be used
    /// by Run().
    void SetEnvBlock(void *env_block)
    {
        env_block_ = env_block;
    }

    /// Start a process and gather its raw output.  Returns its exit code.
    /// Crashes (calls Fatal()) on error.
    static int Run(const std::string &command, std::string *output)
    {
        int stdout_pipe[2], stderr_pipe[2];
        int status;

        // Create pipes for stdout and stderr
        if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1)
        {
            printErrorMessage("Error Creating Pipes\n");
        }

        if (const pid_t pid = fork(); pid == -1)
        {
            printErrorMessage("fork");
        }
        else
        {
            if (pid == 0)
            {
                // Child process

                // Redirect stdout and stderr to the pipes
                dup2(stdout_pipe[1], STDOUT_FILENO); // Redirect stdout to stdout_pipe
                dup2(stderr_pipe[1], STDERR_FILENO); // Redirect stderr to stderr_pipe

                // Close unused pipe ends
                close(stdout_pipe[0]);
                close(stderr_pipe[0]);
                close(stdout_pipe[1]);
                close(stderr_pipe[1]);

                // Execute a command (e.g., "ls" or any other)
                exit(WEXITSTATUS(system(command.c_str())));
            }

            // Parent process
            // Close unused pipe ends
            close(stdout_pipe[1]);
            close(stderr_pipe[1]);

            if (waitpid(pid, &status, 0) < 0)
            {
                printErrorMessage("waitpid");
            }

            char buffer[4096];
            while (true)
            {
                const uint64_t readSize = read(stdout_pipe[0], buffer, sizeof(buffer) - 1);
                if (readSize)
                {
                    output->append(buffer, readSize);
                }
                else
                {
                    break;
                }
            }

            while (true)
            {
                const uint64_t readSize = read(stderr_pipe[0], buffer, sizeof(buffer) - 1);
                if (readSize)
                {
                    output->append(buffer, readSize);
                }
                else
                {
                    break;
                }
            }

            // Close the read ends of the pipes
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
        }
        return WEXITSTATUS(status);
    }

    void *env_block_;
};

#endif

using std::ofstream, fmt::format, std::stringstream;

string getThreadId()
{
    const auto myId = std::this_thread::get_id();
    stringstream ss;
    ss << myId;
    string threadId = ss.str();
    return threadId;
}

RunCommand::RunCommand(path toolPath, const string &runCommand, string printCommand_, bool isTarget_)
    : printCommand(std::move(printCommand_))
{

#ifdef _WIN32

    string j = addQuotes(toolPath.string()) + ' ' + runCommand;
    {
        exitStatus = CLWrapper{}.Run(j, &commandOutput);
    }

#else
    string j = addQuotes(toolPath.string()) + ' ' + runCommand;
    exitStatus = CLWrapper::Run(j, &commandOutput);
#endif
    if (exitStatus != EXIT_SUCCESS)
    {
        bool breakpoint = true;
    }
}

void RunCommand::executePrintRoutine(uint32_t color, TargetCache *target, void *cache) const
{
    bool notify = false;

    {
        std::lock_guard _(targetCacheDiskWriteManager.vecMutex);
        if (exitStatus == EXIT_SUCCESS)
        {
            targetCacheDiskWriteManager.updatedCaches.emplace_back(target, cache);
            notify = true;
        }

        // TODO
        // these print commands formatting should be outside the mutex.
        targetCacheDiskWriteManager.strCache.emplace_back(FORMAT("{}", printCommand + " " + getThreadId() + "\n"),
                                                          color, true);

        if (!commandOutput.empty())
        {
            targetCacheDiskWriteManager.strCache.emplace_back(FORMAT("{}", commandOutput + "\n"),
                                                              static_cast<int>(fmt::color::light_green), true);
            notify = true;
        }
    }
    if (notify)
    {
        targetCacheDiskWriteManager.vecCond.notify_one();
    }
}

inline mutex roundOneMutex;
void RunCommand::executePrintRoutineRoundOne(const SMFile &smFile) const
{
    lock_guard _{roundOneMutex};
    if (exitStatus != EXIT_SUCCESS)
    {
        printErrorMessageNoReturn(
            FORMAT("Scanning Failed for {} of Target {}\n", smFile.node->filePath, smFile.target->name));
        printErrorMessageNoReturn(FORMAT("{}", commandOutput));
    }
}

PostCompile::PostCompile(const CppSourceTarget &target_, const path &toolPath, const string &commandFirstHalf,
                         string printCommandFirstHalf)
    : RunCommand(toolPath, commandFirstHalf, std::move(printCommandFirstHalf), false),
      target{const_cast<CppSourceTarget &>(target_)}
{
}

bool PostCompile::ignoreHeaderFile(const string_view child) const
{
    //  Premature Optimization Hahacd
    // TODO:
    //  Add a key in hconfigure that informs hbuild that the library isn't to be
    //  updated, so includes from the dirs coming from it aren't mentioned
    //  in targetCache and neither are those libraries checked for an edit for
    //  faster startup times.

    // If a file is in environment includes, it is not marked as dependency as an
    // optimization. If a file is in subdir of environment include, it is
    // still marked as dependency. It is not checked if any of environment
    // includes is related(equivalent, subdir) with any of normal includes
    // or vice versa.

    // std::path::equivalent is not used as it is slow
    // It is assumed that both paths are normalized strings
    for (const InclNode &inclNode : target.reqIncls)
    {
        if (inclNode.ignoreHeaderDeps)
        {
            if (childInParentPathNormalized(inclNode.node->filePath, child))
            {
                return true;
            }
        }
    }
    return false;
}

void PostCompile::parseDepsFromMSVCTextOutput(SourceNode &sourceNode, string &output) const
{
    const string includeFileNote = "Note: including file:";

    if (sourceNode.ignoreHeaderDeps && settings.ccpSettings.pruneHeaderDepsFromMSVCOutput)
    {
        // TODO
        //  Merge this if in the following else.
        vector<string> outputLines = split(output, "\n");
        for (auto iter = outputLines.begin(); iter != outputLines.end();)
        {
            if (iter->contains(includeFileNote))
            {
                iter = outputLines.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
    }
    else
    {
        string treatedOutput;

        uint64_t startPos = 0;
        uint64_t lineEnd = output.find('\n');
        if (lineEnd == string::npos)
        {
            return;
        }

        string_view line(output.begin() + startPos, output.begin() + lineEnd + 1);

        if (!settings.ccpSettings.pruneHeaderDepsFromMSVCOutput)
        {
            treatedOutput.append(line);
        }

        startPos = lineEnd + 1;
        if (output.size() == startPos)
        {
            output = std::move(treatedOutput);
            return;
        }
        if (lineEnd > output.size() - 5)
        {
            bool breakpoint = true;
        }
        lineEnd = output.find('\n', startPos);

        vector<Node *> &headerFiles = sourceNode.buildCache.headerFiles;
        headerFiles.clear();
        while (true)
        {

            line = string_view(output.begin() + startPos, output.begin() + lineEnd + 1);
            if (size_t pos = line.find(includeFileNote); pos != string::npos)
            {
                pos = line.find_first_not_of(' ', includeFileNote.size());

                if (line.size() >= pos + 1)
                {
                    // MSVC compiler can output header-includes with / as path separator
                    for (auto it = line.begin() + pos; it != line.end() - 2; ++it)
                    {
                        if (*it == '/')
                        {
                            const_cast<char &>(*it) = '\\';
                        }
                    }

                    // Last character is \r for some reason.
                    string_view headerView{line.begin() + pos, line.end() - 2};

                    // TODO
                    // If compile-command is all lower-cased, then this might not be needed
                    // Some compilers can input same header-file twice, if that is the case, then we should first make
                    // the array unique.
                    if (!ignoreHeaderFile(headerView))
                    {
                        lowerCasePStringOnWindows(const_cast<char *>(headerView.data()), headerView.size());

                        if (Node *headerNode = Node::getHalfNode(headerView);
                            std::ranges::find(headerFiles, headerNode) == headerFiles.end())
                        {
                            headerFiles.emplace_back(headerNode);
                        }
                    }
                }
                else
                {
                    printErrorMessage(FORMAT("Empty Header Include {}\n", line));
                }

                if (!settings.ccpSettings.pruneHeaderDepsFromMSVCOutput)
                {
                    treatedOutput.append(line);
                }
            }
            else
            {
                treatedOutput.append(line);
            }

            startPos = lineEnd + 1;
            if (output.size() == startPos)
            {
                output = std::move(treatedOutput);
                break;
            }
            /*if (lineEnd > output.size() - 5)
            {
                bool breakpoint = true;
            }*/
            lineEnd = output.find('\n', startPos);
        }
    }
}

void PostCompile::parseDepsFromGCCDepsOutput(SourceNode &sourceNode) const
{
    if (!sourceNode.ignoreHeaderDeps)
    {
        const string headerFileContents =
            fileToPString(target.buildCacheFilesDirPathNode->filePath + slashc + sourceNode.node->getFileName() + ".d");
        vector<string> headerDeps = split(headerFileContents, "\n");

        // The First 2 lines are skipped as these are .o and .cpp file.
        // If the file is preprocessed, it does not generate the extra line
        const auto endIt = headerDeps.end() - 1;

        if (headerDeps.size() > 2)
        {
            sourceNode.buildCache.headerFiles.clear();
            for (auto iter = headerDeps.begin() + 2; iter != endIt; ++iter)
            {
                const size_t pos = iter->find_first_not_of(" ");
                auto it = iter->begin() + pos;
                if (const string_view headerView{&*it, iter->size() - (iter->ends_with('\\') ? 2 : 0) - pos};
                    !ignoreHeaderFile(headerView))
                {
                    sourceNode.buildCache.headerFiles.emplace_back(Node::getHalfNode(headerView));
                }
            }
        }
    }
}

void PostCompile::parseHeaderDeps(SourceNode &sourceNode)
{
    if (target.configuration->compilerFeatures.compiler.bTFamily == BTFamily::MSVC)
    {
        parseDepsFromMSVCTextOutput(sourceNode, commandOutput);
    }
    else
    {
        if (exitStatus == EXIT_SUCCESS)
        {
            parseDepsFromGCCDepsOutput(sourceNode);
        }
    }
}