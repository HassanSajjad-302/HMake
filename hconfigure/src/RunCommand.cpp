
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

#ifdef WIN32
#include <Windows.h>

// Copied From Ninja code-base.
/// Wraps a synchronous execution of a CL subprocess.
struct CLWrapper
{
    CLWrapper() : env_block_(NULL)
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
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                   err, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), (char *)&msg_buf, 0, NULL);

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

    if (!CreateProcessA(NULL, (char *)command.c_str(), NULL, NULL,
                        /* inherit handles */ TRUE, 0, env_block_, NULL, &startup_info, &process_info))
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
        if (!::ReadFile(stdout_read, buf, sizeof(buf), &read_len, NULL) && GetLastError() != ERROR_BROKEN_PIPE)
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
    CLWrapper() : env_block_(NULL)
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
            throw std::runtime_error("Error Creating Pipes");
        }

        if (const pid_t pid = fork(); pid == -1)
        {
            printErrorMessage("fork");
            throw std::runtime_error("fork");
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
                throw std::runtime_error("waitpid");
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

using std::ofstream, fmt::format;

pstring getThreadId()
{
    const auto myId = std::this_thread::get_id();
    pstringstream ss;
    ss << myId;
    pstring threadId = ss.str();
    return threadId;
}

RunCommand::RunCommand(path toolPath, const pstring &runCommand, pstring printCommand_, bool isTarget_)
    : printCommand(std::move(printCommand_))
{

#ifdef _WIN32

    pstring j = addQuotes(toolPath.make_preferred().string()) + ' ' + runCommand;
    {
        exitStatus = CLWrapper{}.Run(j, &commandOutput);
    }

#else
    pstring j = addQuotes(toolPath.make_preferred().string()) + ' ' + runCommand;
    exitStatus = CLWrapper::Run(j, &commandOutput);
#endif
    if (exitStatus != EXIT_SUCCESS)
    {
        bool breakpoint = true;
    }
}
void RunCommand::executePrintRoutine(uint32_t color, const bool printOnlyOnError, PValue sourceJson, uint64_t _index0,
                                     uint64_t _index1, uint64_t _index2, uint64_t _index3, uint64_t _index4) const
{
    bool notify = false;
    {

        lock_guard _(targetCacheDiskWriteManager.vecMutex);

        if (exitStatus == EXIT_SUCCESS)
        {
            targetCacheDiskWriteManager.pValueCache.emplace_back(std::move(sourceJson), _index0, _index1, _index2,
                                                                 _index3, _index4);
            notify = true;
        }

        if (printOnlyOnError)
        {
            targetCacheDiskWriteManager.strCache.emplace_back(
                fmt::format("{}", printCommand + " " + getThreadId() + "\n"), color, true);
            notify = true;
            if (exitStatus != EXIT_SUCCESS)
            {
                if (!commandOutput.empty())
                {
                    targetCacheDiskWriteManager.strCache.emplace_back(fmt::format("{}", commandOutput + "\n"),
                                                                      settings.pcSettings.toolErrorOutput, true);
                    notify = true;
                }
            }
        }
        else
        {

            targetCacheDiskWriteManager.strCache.emplace_back(
                fmt::format("{}", printCommand + " " + getThreadId() + "\n"), color, true);

            if (!commandOutput.empty())
            {
                targetCacheDiskWriteManager.strCache.emplace_back(fmt::format("{}", commandOutput + "\n"),
                                                                  static_cast<int>(fmt::color::light_green), true);
                notify = true;
            }
        }
    }

    if (notify)
    {
        targetCacheDiskWriteManager.vecCond.notify_one();
    }
}

PostCompile::PostCompile(const CppSourceTarget &target_, const path &toolPath, const pstring &commandFirstHalf,
                         pstring printCommandFirstHalf)
    : RunCommand(toolPath, commandFirstHalf, std::move(printCommandFirstHalf), false),
      target{const_cast<CppSourceTarget &>(target_)}
{
}

bool PostCompile::ignoreHeaderFile(const pstring_view child) const
{
    //  Premature Optimization Hahacd
    // TODO:
    //  Add a key in hconfigure that informs hbuild that the library isn't to be
    //  updated, so includes from the directories coming from it aren't mentioned
    //  in targetCache and neither are those libraries checked for an edit for
    //  faster startup times.

    // If a file is in environment includes, it is not marked as dependency as an
    // optimization. If a file is in subdirectory of environment include, it is
    // still marked as dependency. It is not checked if any of environment
    // includes is related(equivalent, subdirectory) with any of normal includes
    // or vice-versa.

    // std::path::equivalent is not used as it is slow
    // It is assumed that both paths are normalized strings
    for (const InclNode &inclNode : target.reqIncls)
    {
        if (inclNode.ignoreHeaderDeps)
        {
            if (childInParentPathRecursiveNormalized(inclNode.node->filePath, child))
            {
                return true;
            }
        }
    }
    return false;
}

void PostCompile::parseDepsFromMSVCTextOutput(SourceNode &sourceNode, pstring &output, PValue &headerDepsJson,
                                              const bool mustConsiderHeaderDeps) const
{
    // TODO
    //  Maybe use pstring_view instead
    vector<pstring> outputLines = split(output, "\n");
    const pstring includeFileNote = "Note: including file:";

    if (!mustConsiderHeaderDeps && sourceNode.ignoreHeaderDeps)
    {
        // TODO
        // Unroll this loop.
        for (auto iter = outputLines.begin(); iter != outputLines.end();)
        {
            if (iter->contains(includeFileNote))
            {
                if (settings.ccpSettings.pruneHeaderDepsFromMSVCOutput)
                {
                    iter = outputLines.erase(iter);
                }
            }
            else
            {
                ++iter;
            }
        }
    }
    else
    {
        for (auto iter = outputLines.begin(); iter != outputLines.end();)
        {
            if (size_t pos = iter->find(includeFileNote); pos != pstring::npos)
            {
                pos = iter->find_first_not_of(' ', includeFileNote.size());

                if (iter->size() >= pos + 1)
                {
                    // Last character is \r for some reason.
                    iter->back() = '\0';
                    pstring_view headerView(iter->begin() + pos, iter->end() - 1);

                    // TODO
                    // If compile-command is all lower-cased, then this might not be needed
                    if (mustConsiderHeaderDeps || !ignoreHeaderFile(headerView))
                    {
                        lowerCasePStringOnWindows(const_cast<pchar *>(headerView.data()), headerView.size());

#ifdef USE_NODES_CACHE_INDICES_IN_CACHE
                        Node *node = Node::getHalfNodeFromNormalizedString(headerView);
                        bool found = false;
                        for (const PValue &value : headerDepsJson.GetArray())
                        {
                            if (value.GetUint64() == node->myId)
                            {
                                found = true;
                                break;
                            }
                        }
                        if (!found)
                        {
                            headerDepsJson.PushBack(PValue(node->myId), sourceNode.sourceNodeAllocator);
                        }

#else

                        // We need node so that it gets registered in the central nodes.cache
                        const Node *headerNode = Node::getHalfNodeFromNormalizedString(headerView);
                        bool found = false;
                        for (const PValue &value : headerDepsJson.GetArray())
                        {
                            if (compareStringsFromEnd(pstring_view(value.GetString(), value.GetStringLength()),
                                                      headerNode->filePath))
                            {
                                found = true;
                                break;
                            }
                        }
                        if (!found)
                        {
                            headerDepsJson.PushBack(PValue(PStringRef(headerView.data(), headerView.size()),
                                                           sourceNode.sourceNodeAllocator),
                                                    sourceNode.sourceNodeAllocator);
                        }

#endif
                    }
                }
                else
                {
                    printErrorMessage(fmt::format("Empty Header Include {}\n", *iter));
                }

                if (settings.ccpSettings.pruneHeaderDepsFromMSVCOutput)
                {
                    iter = outputLines.erase(iter);
                }
            }
            else
            {
                ++iter;
            }
        }
    }

    if (settings.ccpSettings.pruneCompiledSourceFileNameFromMSVCOutput)
    {
        if (!outputLines.empty())
        {
            outputLines.erase(outputLines.begin());
        }
    }

    pstring treatedOutput; // Output With All information of include files removed.
    for (const auto &i : outputLines)
    {
        treatedOutput += i + "\n";
    }
    if (!treatedOutput.empty())
    {
        treatedOutput.pop_back();
    }
    output = std::move(treatedOutput);
}

void PostCompile::parseDepsFromGCCDepsOutput(SourceNode &sourceNode, PValue &headerDepsJson,
                                             const bool mustConsiderHeaderDeps)
{
    if (mustConsiderHeaderDeps || !sourceNode.ignoreHeaderDeps)
    {
        const pstring headerFileContents = fileToPString(target.buildCacheFilesDirPath +
                                                         (path(sourceNode.node->filePath).filename().*toPStr)() + ".d");
        vector<pstring> headerDeps = split(headerFileContents, "\n");

        // First 2 lines are skipped as these are .o and .cpp file.
        // If the file is preprocessed, it does not generate the extra line
        const auto endIt =
            headerDeps.end() - (sourceNode.target->compileTargetType == TargetType::LIBRARY_OBJECT ? 1 : 0);

        if (headerDeps.size() > 2)
        {
            for (auto iter = headerDeps.begin() + 2; iter != endIt; ++iter)
            {
                const size_t pos = iter->find_first_not_of(" ");
                auto it = iter->begin() + pos;
                if (const pstring_view headerView{&*it, iter->size() - (iter->ends_with('\\') ? 2 : 0) - pos};
                    mustConsiderHeaderDeps || !ignoreHeaderFile(headerView))
                {
                    const Node *headerNode = Node::getHalfNodeFromNormalizedString(headerView);
                    headerDepsJson.PushBack(headerNode->getPValue(), sourceNode.sourceNodeAllocator);
                }
            }
        }
    }
}

void PostCompile::parseHeaderDeps(SourceNode &sourceNode, const bool mustConsiderHeaderDeps)
{
    PValue &headerDepsJson = sourceNode.sourceJson[Indices::CppTarget::BuildCache::SourceFiles::headerFiles];
    headerDepsJson.Clear();

    if (target.compiler.bTFamily == BTFamily::MSVC)
    {
        parseDepsFromMSVCTextOutput(sourceNode, commandOutput, headerDepsJson, mustConsiderHeaderDeps);
    }
    else
    {
        if (exitStatus == EXIT_SUCCESS)
        {
            parseDepsFromGCCDepsOutput(sourceNode, headerDepsJson, mustConsiderHeaderDeps);
        }
    }
}