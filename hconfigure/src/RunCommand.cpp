
#include "RunCommand.hpp"

#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "IPCManagerBS.hpp"

#include <utility>

#ifndef _WIN32
#include "sys/wait.h"
#include "wordexp.h"
#include <fcntl.h>
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
uint64_t RunCommand::startAsyncProcess(const char *command, Builder &builder, BTarget *bTarget, bool haveWritePipe)
{
    const string readPipeName =
        FORMAT("\\\\.\\pipe\\{}", std::chrono::high_resolution_clock::now().time_since_epoch().count());

    HANDLE readPipeHandle = CreateNamedPipeA(readPipeName.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                             PIPE_TYPE_BYTE, PIPE_UNLIMITED_INSTANCES, 0, 0, INFINITE, NULL);
    if (readPipeHandle == INVALID_HANDLE_VALUE)
    {
        printErrorMessage(P2978::getErrorString());
    }

    index = builder.registerEventData(bTarget, (uint64_t)readPipeHandle);
    if (!CreateIoCompletionPort(readPipeHandle, (HANDLE)builder.serverFd, index, 0))
    {
        printErrorMessage(P2978::getErrorString());
    }

    if (CompletionKey &k = eventData[index];
        !ConnectNamedPipe(readPipeHandle, &reinterpret_cast<OVERLAPPED &>(k.overlappedBuffer)) &&
        GetLastError() != ERROR_IO_PENDING)
    {
        printErrorMessage(P2978::getErrorString());
    }

    // Get the write end of the pipe as a handle inheritable across processes.
    HANDLE outputWriteHandle =
        CreateFileA(readPipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE child_pipe;
    if (!DuplicateHandle(GetCurrentProcess(), outputWriteHandle, GetCurrentProcess(), &child_pipe, 0, TRUE,
                         DUPLICATE_SAME_ACCESS))
    {
        printErrorMessage(P2978::getErrorString());
    }
    CloseHandle(outputWriteHandle);

    STARTUPINFOA startup_info;
    memset(&startup_info, 0, sizeof(startup_info));
    startup_info.cb = sizeof(STARTUPINFO);

    bool use_console_ = false;
    if (!use_console_)
    {
        startup_info.dwFlags = STARTF_USESTDHANDLES;
        startup_info.hStdInput = child_pipe;
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
        printErrorMessage(FORMAT("CreateProcessA failed for Command\n{}\nError\n", command, P2978::getErrorString()));
    }

    // Close pipe channel only used by the child.
    if (child_pipe)
    {
        CloseHandle(child_pipe);
    }

    CloseHandle(process_info.hThread);

    // todo
    // don't inherit the parent fds.
    /*
    HANDLE hd = (HANDLE) _get_osfhandle(fd);
    if (! SetHandleInformation(hd, HANDLE_FLAG_INHERIT, 0)) {
        fprintf(stderr, "SetHandleInformation(): %s", GetLastErrorString().c_str());
    }
    */
    readPipe = (uint64_t)readPipeHandle;
    writePipe = (uint64_t)readPipeHandle;
    pid = (uint64_t)process_info.hProcess;

    return index;
}

bool RunCommand::startRead()
{
    CompletionKey &k = eventData[index];
    memset(k.buffer, 0, sizeof(k.buffer));
    memset(k.overlappedBuffer, 0, sizeof(k.overlappedBuffer));
    DWORD bytesRead = 0;
    const BOOL result =
        ReadFile((HANDLE)k.handle, k.buffer, 4096, &bytesRead, &reinterpret_cast<OVERLAPPED &>(k.overlappedBuffer));
    if (result)
    {
        // Event if synchronous succeeded, we need to process the stale completion event sent to the event loop.
        return false;
    }
    const DWORD error = GetLastError();
    if (error == ERROR_IO_PENDING)
    {
        return false;
    }
    if (error == ERROR_BROKEN_PIPE)
    {
        // ReadFile completed successfully
        return true;
    }

    printErrorMessage(P2978::getErrorString());
    return true;
}

CompleteReadType RunCommand::completeRead()
{
    CompletionKey &k = eventData[index];
    uint64_t bytesRead = 0;
    if (!GetOverlappedResult((HANDLE)k.handle, &(OVERLAPPED &)k.overlappedBuffer, (LPDWORD)&bytesRead, false))
    {
        if (GetLastError() == ERROR_BROKEN_PIPE)
        {
            // ReadFile completed successfully
            return CompleteReadType::COMPLETE_PROCESS;
        }
        printErrorMessage(P2978::getErrorString());
    }

    output.append(string_view{k.buffer, bytesRead});
    if (output.ends_with(P2978::delimiter))
    {
        return CompleteReadType::COMPLETE_MESSAGE;
    }

    return CompleteReadType::INCOMPLETE;
}

void RunCommand::reapProcess()
{
    if (WaitForSingleObject((HANDLE)pid, INFINITE) == WAIT_FAILED)
    {
        printErrorMessage(P2978::getErrorString());
    }
    if (!GetExitCodeProcess((HANDLE)pid, (LPDWORD)&exitStatus))
    {
        printErrorMessage(P2978::getErrorString());
    }

    if (!CloseHandle((HANDLE)pid) || !CloseHandle((HANDLE)readPipe))
    {
        printErrorMessage(P2978::getErrorString());
    }
}

void RunCommand::killModuleProcess(Builder &builder) const
{
    // Exit code you want to assign to the terminated process
    DWORD exitCode = 1;

    if (!TerminateProcess((HANDLE)pid, exitCode))
    {
        printErrorMessage(("Killing Process\n"));
    }

    CloseHandle((HANDLE)pid); // Clean up when you’re done
    builder.unregisterEventDataAtIndex(index);
}

#else

// todo
//  unaccounted for close() calls.

uint64_t RunCommand::startAsyncProcess(const char *command, Builder &builder, BTarget *bTarget, bool haveWritePipe)
{
    wordexp_t p;
    if (wordexp(command, &p, 0) != 0)
    {
        printErrorMessage("wordexp failed\n");
        return -1;
    }

    int stdoutPipesLocal[2];
    if (pipe2(stdoutPipesLocal, O_CLOEXEC) == -1)
    {
        printErrorMessage(P2978::getErrorString());
    }
    readPipe = stdoutPipesLocal[0];

    int stdinPipesLocal[2];
    if (haveWritePipe)
    {
        if (pipe2(stdinPipesLocal, O_CLOEXEC) == -1)
            printErrorMessage(P2978::getErrorString());
        writePipe = stdinPipesLocal[1];
    }
    else
    {
        // todo
        // in child redirect /dev/null for both Windows and Linux.
    }

    pid = vfork(); // <-- vfork
    if (pid == -1)
    {
        printErrorMessage(P2978::getErrorString());
    }

    if (pid == 0)
    {
        // Child — only dup2, close, execvp and _exit allowed
        dup2(stdoutPipesLocal[1], STDOUT_FILENO);
        dup2(stdoutPipesLocal[1], STDERR_FILENO);
        close(stdoutPipesLocal[0]);
        close(stdoutPipesLocal[1]);

        if (haveWritePipe)
        {
            dup2(stdinPipesLocal[0], STDIN_FILENO);
            close(stdinPipesLocal[0]);
            close(stdinPipesLocal[1]);
        }

        execvp(p.we_wordv[0], p.we_wordv);
        _exit(127); // must use _exit, never exit()
    }

    // Parent
    close(stdoutPipesLocal[1]);
    wordfree(&p); // safe to free here, child has already exec'd
    builder.registerEventData(bTarget, readPipe);

    if (haveWritePipe)
    {
        close(stdinPipesLocal[0]);
    }

    ++builder.simultaneousProcessCount;
    return readPipe;
}

bool RunCommand::startRead()
{
    return false;
}

CompleteReadType RunCommand::completeRead()
{
    char buffer[64 * 1024];
    const uint64_t readSize = read(readPipe, buffer, sizeof(buffer) - 1);
    if (readSize == -1)
    {
        printErrorMessage(P2978::getErrorString());
    }
    if (!readSize)
    {
        return CompleteReadType::COMPLETE_PROCESS;
    }
    output.append(buffer, readSize);
    if (output.ends_with(P2978::delimiter))
    {
        return CompleteReadType::COMPLETE_MESSAGE;
    }
    return CompleteReadType::INCOMPLETE;
}

void RunCommand::runProcess(const char *command)
{
    // Create pipes for stdout and stderr
    int stdPipesLocal[2];
    if (pipe(stdPipesLocal) == -1)
    {
        printErrorMessage("Error Creating Pipes\n");
    }

    readPipe = stdPipesLocal[0];

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
        if (const uint64_t readSize = read(readPipe, buffer, sizeof(buffer) - 1))
        {
            output.append(buffer, readSize);
        }
        else
        {
            break;
        }
    }

    // Close the read ends of the pipes
    close(readPipe);

    int status;
    if (waitpid(pid, &status, 0) < 0)
    {
        printErrorMessage("waitpid");
    }
    exitStatus = WEXITSTATUS(status);
}

void RunCommand::reapProcess(Builder &builder)
{
    --builder.simultaneousProcessCount;
    int status;
    if (waitpid(pid, &status, 0) < 0)
    {
        printErrorMessage(P2978::getErrorString());
    }
    if (close(readPipe) == -1)
    {
        printErrorMessage(P2978::getErrorString());
    }

    if (writePipe != -1)
    {
        if (close(writePipe) == -1)
        {
            printErrorMessage(P2978::getErrorString());
        }
    }
    exitStatus = WEXITSTATUS(status); // extract actual exit code
}

void RunCommand::killModuleProcess(Builder &builder) const
{
    --builder.simultaneousProcessCount;
    if (kill(pid, SIGKILL) != 0)
    {
        printErrorMessage("Killing process");
    }
    builder.unregisterEventDataAtIndex(readPipe);
}

#endif

#ifdef _WIN32

#endif

string RunCommand::pruneOutput()
{
    // Prune the compiler output. and make a new string of the compiler-message output.
    const uint32_t outputSize = output.size();
    if (outputSize < 4 + strlen(P2978::delimiter))
    {
        printErrorMessage("Received string only has delimiter but not the size of payload\n");
    }

    const uint32_t payloadSize =
        *reinterpret_cast<uint32_t *>(output.data() + (outputSize - (4 + strlen(P2978::delimiter))));
    const char *payloadStart = output.data() + (outputSize - (4 + strlen(P2978::delimiter) + payloadSize));

    string str{payloadStart, payloadSize};
    output.resize(outputSize - (4 + strlen(P2978::delimiter) + payloadSize));

    return str;
}
