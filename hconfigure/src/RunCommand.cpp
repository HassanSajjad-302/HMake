
#include "RunCommand.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "IPCManagerBS.hpp"
#ifdef _WIN32
#else
#include "sys/wait.h"
#include "wordexp.h"
#endif

#ifdef _WIN32
#include <Windows.h>
#include <chrono>

void RunCommand::runProcess(const char *command)
{
    // Generate unique temp filenames using timestamp
    auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    string stdoutFile = FORMAT("temp_stdout_{}.txt", timestamp);
    string stderrFile = FORMAT("temp_stderr_{}.txt", timestamp);

#ifdef _WIN32
    // Windows: Use cmd.exe /c with proper quoting
    const string finalCommand = FORMAT("cmd.exe /c \"({}) > {} 2> {}\"", command, stdoutFile, stderrFile);
#else
    // Unix/Linux/macOS: Standard shell redirection works fine
    const string finalCommand = FORMAT("{} > {} 2> {}", command, stdoutFile, stderrFile);
#endif

    exitStatus = system(finalCommand.c_str());

    output = fileToString(stdoutFile);
    const string errorFileContent = fileToString(stderrFile);
    if (!output.empty() && !errorFileContent.empty())
    {
        output += "\n--- STDERR ---\n";
    }
    output += errorFileContent;

    std::filesystem::remove(stdoutFile);
    std::filesystem::remove(stderrFile);
}

// Copied partially from Ninja
uint64_t RunCommand::startAsyncProcess(const char *command, Builder &builder, BTarget *bTarget)
{
    // One BTarget can launch multiple processes so we also append the clock::now().
    const string pipe_name =
        FORMAT("\\\\.\\pipe\\{}{}", bTarget->id, std::chrono::high_resolution_clock::now().time_since_epoch().count());
    HANDLE pipe_ = CreateNamedPipeA(pipe_name.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE,
                                    PIPE_UNLIMITED_INSTANCES, 0, 0, INFINITE, NULL);
    if (pipe_ == INVALID_HANDLE_VALUE)
    {
        printErrorMessage(N2978::getErrorString());
    }

    const uint64_t eventDataIndex = builder.registerEventData(bTarget, (uint64_t)pipe_);
    if (!CreateIoCompletionPort(pipe_, (HANDLE)builder.serverFd, eventDataIndex, 0))
    {
        printErrorMessage(N2978::getErrorString());
    }

    if (CompletionKey &k = eventData[eventDataIndex];
        !ConnectNamedPipe(pipe_, &reinterpret_cast<OVERLAPPED &>(k.overlappedBuffer)) &&
        GetLastError() != ERROR_IO_PENDING)
    {
        printErrorMessage(N2978::getErrorString());
    }

    // Get the write end of the pipe as a handle inheritable across processes.
    HANDLE output_write_handle = CreateFileA(pipe_name.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE child_pipe;
    if (!DuplicateHandle(GetCurrentProcess(), output_write_handle, GetCurrentProcess(), &child_pipe, 0, TRUE,
                         DUPLICATE_SAME_ACCESS))
    {
        printErrorMessage(N2978::getErrorString());
    }
    CloseHandle(output_write_handle);

    SECURITY_ATTRIBUTES security_attributes;
    memset(&security_attributes, 0, sizeof(SECURITY_ATTRIBUTES));
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle = TRUE;
    // Must be inheritable so subprocesses can dup to children.
    HANDLE nul = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             &security_attributes, OPEN_EXISTING, 0, NULL);
    if (nul == INVALID_HANDLE_VALUE)
    {
        printErrorMessage(N2978::getErrorString());
    }

    STARTUPINFOA startup_info;
    memset(&startup_info, 0, sizeof(startup_info));
    startup_info.cb = sizeof(STARTUPINFO);

    bool use_console_ = false;
    if (!use_console_)
    {
        startup_info.dwFlags = STARTF_USESTDHANDLES;
        startup_info.hStdInput = nul;
        startup_info.hStdOutput = child_pipe;
        startup_info.hStdError = child_pipe;
    }
    // In the console case, child_pipe is still inherited by the child and closed
    // when the subprocess finishes, which then notifies ninja.

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    // Ninja handles ctrl-c, except for subprocesses in console pools.
    DWORD process_flags = use_console_ ? 0 : CREATE_NEW_PROCESS_GROUP;

    // Do not prepend 'cmd /c' on Windows, this breaks command
    // lines greater than 8,191 chars.
    if (!CreateProcessA(NULL, (char *)command, NULL, NULL,
                        /* inherit handles */ TRUE, process_flags, NULL, NULL, &startup_info, &process_info))
    {
        printErrorMessage(FORMAT("CreateProcessA failed for Command\n{}\nError\n", command, N2978::getErrorString()));
    }

    // Close pipe channel only used by the child.
    if (child_pipe)
    {
        CloseHandle(child_pipe);
    }
    CloseHandle(nul);

    CloseHandle(process_info.hThread);

    stdPipe = (uint64_t)pipe_;
    pid = (uint64_t)process_info.hProcess;

    return eventDataIndex;
}

bool RunCommand::wasProcessLaunchIncomplete(const uint64_t index)
{
#ifdef _WIN32
    if (processState == ProcessState::LAUNCHED)
    {
        CompletionKey &k = eventData[index];
        uint64_t bytesRead = 0;
        const bool result =
            GetOverlappedResult((HANDLE)k.handle, &(OVERLAPPED &)k.overlappedBuffer, (LPDWORD)&bytesRead, false);
        if (result)
        {
            processState = ProcessState::OUTPUT_CONNECTED;
            return true;
        }
        if (GetLastError() == ERROR_BROKEN_PIPE)
        {
            processState = ProcessState::COMPLETED;
            return true;
        }
        printErrorMessage(N2978::getErrorString());
    }
    return false;
#else
    return false;
#endif
}

void RunCommand::reapProcess() const
{
    if (WaitForSingleObject((HANDLE)pid, INFINITE) == WAIT_FAILED)
    {
        printErrorMessage(N2978::getErrorString());
    }
    if (!GetExitCodeProcess((HANDLE)pid, (LPDWORD)&exitStatus))
    {
        printErrorMessage(N2978::getErrorString());
    }

    if (!CloseHandle((HANDLE)pid) || !CloseHandle((HANDLE)stdPipe))
    {
        printErrorMessage(N2978::getErrorString());
    }
}

void RunCommand::killModuleProcess(const string &processName) const
{
    // Exit code you want to assign to the terminated process
    DWORD exitCode = 1;

    if (!TerminateProcess((HANDLE)pid, exitCode))
    {
        printErrorMessage(FORMAT("Killing module process {} failed.\n", processName));
    }

    CloseHandle((HANDLE)pid); // Clean up when you’re done
}

#else

uint64_t RunCommand::startAsyncProcess(const char *command, Builder &builder, BTarget *bTarget)
{
    // Create pipes for stdout and stderr
    int stdPipesLocal[2];
    if (pipe(stdPipesLocal) == -1)
    {
        printErrorMessage("Error Creating Pipes\n");
    }

    stdPipe = stdPipesLocal[0];

    pid = fork();
    if (pid == -1)
    {
        printErrorMessage("fork");
    }
    if (pid == 0)
    {
        // Child process

        // Redirect stdout and stderr to the pipes
        dup2(stdPipesLocal[1], STDOUT_FILENO); // Redirect stdout to stdout_pipe
        dup2(stdPipesLocal[1], STDERR_FILENO); // Redirect stderr to stderr_pipe

        // Close unused pipe ends
        close(stdPipesLocal[0]);
        close(stdPipesLocal[1]);

        wordexp_t p;
        if (wordexp(command, &p, 0) != 0)
        {
            perror("wordexp");
            _exit(127);
        }

        // p.we_wordv is a NULL-terminated argv suitable for exec*
        char **argv = p.we_wordv;

        // Use execvp so PATH is searched and environment is inherited
        execvp(argv[0], argv);

        // If execvp returns, it failed:
        perror("execvp");
        wordfree(&p);
        _exit(127);
    }

    // Parent process
    // Close unused pipe ends
    close(stdPipesLocal[1]);

    builder.registerEventData(bTarget, stdPipe);
    return stdPipe;
}

bool RunCommand::wasProcessLaunchIncomplete(uint64_t index)
{
    return false;
}

void RunCommand::runProcess(const char *command)
{
    // Create pipes for stdout and stderr
    int stdPipesLocal[2];
    if (pipe(stdPipesLocal) == -1)
    {
        printErrorMessage("Error Creating Pipes\n");
    }

    stdPipe = stdPipesLocal[0];

    pid = fork();
    if (pid == -1)
    {
        printErrorMessage("fork");
    }
    if (pid == 0)
    {
        // Child process

        // Redirect stdout and stderr to the pipes
        dup2(stdPipesLocal[1], STDOUT_FILENO); // Redirect stdout to stdout_pipe
        dup2(stdPipesLocal[1], STDERR_FILENO); // Redirect stderr to stderr_pipe

        // Close unused pipe ends
        close(stdPipesLocal[0]);
        close(stdPipesLocal[1]);

        wordexp_t p;
        if (wordexp(command, &p, 0) != 0)
        {
            perror("wordexp");
            _exit(127);
        }

        // p.we_wordv is a NULL-terminated argv suitable for exec*
        char **argv = p.we_wordv;

        // Use execvp so PATH is searched and environment is inherited
        execvp(argv[0], argv);

        // If execvp returns, it failed:
        perror("execvp");
        wordfree(&p);
        _exit(127);
    }

    // Parent process
    // Close unused pipe ends
    close(stdPipesLocal[1]);

    char buffer[4096 * 16];
    while (true)
    {
        if (const uint64_t readSize = read(stdPipe, buffer, sizeof(buffer) - 1))
        {
            output.append(buffer, readSize);
        }
        else
        {
            break;
        }
    }

    // Close the read ends of the pipes
    close(stdPipe);

    int status;
    if (waitpid(pid, &status, 0) < 0)
    {
        printErrorMessage("waitpid");
    }
    exitStatus = WEXITSTATUS(status);
}

void RunCommand::reapProcess()
{
    if (waitpid(pid, &exitStatus, 0) < 0)
    {
        printErrorMessage("waitpid");
    }
    if (close(stdPipe) == -1)
    {
        printErrorMessage("close failed\n");
    }
}

void RunCommand::killModuleProcess(const string &processName) const
{
    if (kill(pid, SIGKILL) != 0)
    {
        printErrorMessage(FORMAT("Killing module process {} failed.\n", processName));
    }
}

#endif
