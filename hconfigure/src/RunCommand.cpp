
#include "RunCommand.hpp"
#include "BuildSystemFunctions.hpp"
#ifdef _WIN32
#else
#include "sys/wait.h"
#endif

#ifdef _WIN32
#include <Windows.h>

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

void RunCommand::startProcess(const string &command, bool isModuleProcess)
{
    PROCESS_INFORMATION process_info = {};
    STARTUPINFOA startup_info = {};

    if (isModuleProcess)
    {
        if (!CreateProcessA(nullptr, (char *)command.c_str(), nullptr, nullptr,
                            /* inherit handles */ TRUE, 0, nullptr, nullptr, &startup_info, &process_info))
        {
            Win32Fatal("CreateProcess");
        }
        hProcess = process_info.hProcess;
        return;
    }

    SECURITY_ATTRIBUTES security_attributes = {};
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle = TRUE;

    HANDLE stdout_write;
    if (!CreatePipe(&stdout_read, &stdout_write, &security_attributes, 0))
        Win32Fatal("CreatePipe");

    if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0))
        Win32Fatal("SetHandleInformation");

    startup_info.cb = sizeof(STARTUPINFOA);
    startup_info.hStdError = stdout_write;
    startup_info.hStdOutput = stdout_write;
    startup_info.dwFlags |= STARTF_USESTDHANDLES;

    if (!CreateProcessA(nullptr, (char *)command.c_str(), nullptr, nullptr,
                        /* inherit handles */ TRUE, 0, nullptr, nullptr, &startup_info, &process_info))
    {
        Win32Fatal("CreateProcess");
    }
    hProcess = process_info.hProcess;
    hThread = process_info.hThread;

    if (!CloseHandle(stdout_write))
    {
        Win32Fatal("CloseHandle");
    }
}

RunCommand::OutputAndStatus RunCommand::endProcess(bool endModuleProcess) const
{
    OutputAndStatus o;
    DWORD exit_code = 0;

    if (endModuleProcess)
    {
        // Wait for it to exit and grab its exit code.
        if (WaitForSingleObject(hProcess, INFINITE) == WAIT_FAILED)
            Win32Fatal("WaitForSingleObject");
        if (!GetExitCodeProcess(hProcess, &exit_code))
            Win32Fatal("GetExitCodeProcess");

        if (!CloseHandle(hProcess))
        {
            Win32Fatal("CloseHandle");
        }
    }
    else
    {
        // Read all output of the subprocess.
        DWORD read_len = 1;
        while (read_len)
        {
            char buf[64 << 10];
            read_len = 0;
            if (const bool out = ReadFile(stdout_read, buf, sizeof(buf), &read_len, nullptr); !out)
            {
                if (GetLastError() == ERROR_BROKEN_PIPE)
                {
                    break;
                }
                Win32Fatal("ReadFile");
            }
            o.output.append(buf, read_len);
        }

        // Wait for it to exit and grab its exit code.
        if (WaitForSingleObject(hProcess, INFINITE) == WAIT_FAILED)
            Win32Fatal("WaitForSingleObject");
        if (!GetExitCodeProcess(hProcess, &exit_code))
            Win32Fatal("GetExitCodeProcess");

        if (!CloseHandle(stdout_read) || !CloseHandle(hProcess) || !CloseHandle(hThread))
        {
            Win32Fatal("CloseHandle");
        }
    }

    o.exitStatus = exit_code;
    return o;
}

void RunCommand::killModuleProcess(const string &processName) const
{
    // Exit code you want to assign to the terminated process
    DWORD exitCode = 1;

    if (!TerminateProcess(hProcess, exitCode))
    {
        printErrorMessage(FORMAT("Killing module process {} failed.\n", processName));
    }

    CloseHandle(hProcess); // Clean up when you’re done
}

#else

void RunCommand::startProcess(const string &command, bool isModuleProcess)
{
    // Create pipes for stdout and stderr
    if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1)
    {
        printErrorMessage("Error Creating Pipes\n");
    }

    pid = fork();
    if (pid == -1)
    {
        printErrorMessage("fork");
    }
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
}

RunCommand::OutputAndStatus RunCommand::endProcess(bool endModuleProcess) const
{

    OutputAndStatus o;

    // Parent process
    // Close unused pipe ends
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    int status;
    if (waitpid(pid, &status, 0) < 0)
    {
        printErrorMessage("waitpid");
    }

    char buffer[4096];
    while (true)
    {
        if (const uint64_t readSize = read(stdout_pipe[0], buffer, sizeof(buffer) - 1))
        {
            o.output.append(buffer, readSize);
        }
        else
        {
            break;
        }
    }

    while (true)
    {
        if (const uint64_t readSize = read(stderr_pipe[0], buffer, sizeof(buffer) - 1))
        {
            o.output.append(buffer, readSize);
        }
        else
        {
            break;
        }
    }

    // Close the read ends of the pipes
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);

    o.exitStatus = WEXITSTATUS(status);
    return o;
}

#endif

RunCommand::RunCommand()
{
    /*
                                #ifdef _WIN32
                                    startProcess(runCommand);
                                #else
                                    exitStatus = CLWrapper::Run(j, &commandOutput);
                                #endif
                                */
}