#include "RunCommand.hpp"

#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "Manager.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <limits>
#include <vector>

#ifndef _WIN32
#include "sys/prctl.h"
#include "sys/wait.h"
#include "wordexp.h"
#include <cerrno>
#include <fcntl.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{
string quoteShellPath(const std::filesystem::path &value)
{
#ifdef _WIN32
    return FORMAT("\"{}\"", value.string());
#else
    const string text = value.string();
    string quoted;
    quoted.reserve(text.size() + 2);
    quoted.push_back('\'');
    for (const char character : text)
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
#endif
}

std::vector<string *> &getOutputPool()
{
    // Asynchronous RunCommand users share Builder's single scheduler thread. Keep the pool alive for the process
    // lifetime so teardown never walks thousands of retained buffers; the OS reclaims them at exit.
    static auto *const pool = [] {
        auto *created = new std::vector<string *>();
        created->reserve(4 * 1024);
        return created;
    }();
    return *pool;
}

#ifdef _WIN32
HANDLE getChildProcessJob()
{
    // A single job contains every asynchronous build child. Attaching it through STARTUPINFOEX makes parent-death
    // containment atomic with CreateProcess and avoids CREATE_SUSPENDED/AssignProcessToJobObject/ResumeThread launch
    // round trips. The process-lifetime handle is deliberately left open; Windows closes it if HMake exits or dies.
    static HANDLE job = [] {
        HANDLE created = CreateJobObjectA(nullptr, nullptr);
        if (!created)
        {
            printErrorMessage(
                FORMAT("Could not create the build child-process job.\nSystem error: {}", P2978::getErrorString()));
        }

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(created, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
        {
            printErrorMessage(FORMAT("Could not configure parent-death handling for build child processes.\n"
                                     "System error: {}",
                                     P2978::getErrorString()));
        }
        return created;
    }();
    return job;
}

HANDLE getWriteCompletionEvent()
{
    // isEventCompleted and isEventRegistered run on Builder's single scheduler thread. Reusing one process-lifetime
    // event avoids a handle allocation for every compiler response without introducing cross-thread synchronization.
    static HANDLE event = [] {
        HANDLE created = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        if (!created)
        {
            printErrorMessage(
                FORMAT("Could not create the child-input write event.\nSystem error: {}", P2978::getErrorString()));
        }
        return created;
    }();
    return event;
}
#endif
} // namespace

RunCommand::~RunCommand()
{
    releaseOutput();
}

void RunCommand::acquireOutput()
{
    if (output)
    {
        // An asynchronous RunCommand may be reused before returning its lease to the pool.
        output->clear();
        return;
    }

    std::vector<string *> &pool = getOutputPool();
    if (pool.empty())
    {
        output = new string();
    }
    else
    {
        output = pool.back();
        pool.pop_back();
    }
    output->clear();
}

void RunCommand::releaseOutput()
{
    if (!output)
    {
        return;
    }
    output->clear();
    getOutputPool().push_back(output);
    output = nullptr;
}

RunCommand::OutputAndStatus RunCommand::runProcess(const char *const command,
                                                   const std::filesystem::path &workingDirectory)
{
    std::error_code error;
    std::filesystem::path directory =
        workingDirectory.empty() ? std::filesystem::current_path(error)
                                 : std::filesystem::absolute(workingDirectory, error);
    if (error || !std::filesystem::is_directory(directory, error))
    {
        printErrorMessage(FORMAT("Could not use the synchronous process working directory.\nDirectory: {}\n"
                                 "System error: {}",
                                 directory.string(), error ? error.message() : "not a directory"));
    }
    directory = directory.lexically_normal();

    error.clear();
    std::filesystem::path captureDirectory =
        workingDirectory.empty() ? std::filesystem::temp_directory_path(error) : directory;
    if (!error)
    {
        captureDirectory = std::filesystem::absolute(captureDirectory, error);
    }
    if (error || !std::filesystem::is_directory(captureDirectory, error))
    {
        printErrorMessage(FORMAT("Could not use the synchronous process capture directory.\nDirectory: {}\n"
                                 "System error: {}",
                                 captureDirectory.string(), error ? error.message() : "not a directory"));
    }
    captureDirectory = captureDirectory.lexically_normal();

#ifdef _WIN32
    const uint64_t processId = GetCurrentProcessId();
#else
    const uint64_t processId = static_cast<uint64_t>(getpid());
#endif
    const string uniqueStem = FORMAT(".hmake-process-{}", processId);
    const std::filesystem::path stdoutFile = captureDirectory / (uniqueStem + "-stdout.txt");
    const std::filesystem::path stderrFile = captureDirectory / (uniqueStem + "-stderr.txt");

#ifdef _WIN32
    const string finalCommand = FORMAT("(cd /d {} && ({})) > {} 2> {}", quoteShellPath(directory), command,
                                       quoteShellPath(stdoutFile), quoteShellPath(stderrFile));
#else
    const string finalCommand = FORMAT("(cd {} && ({})) > {} 2> {}", quoteShellPath(directory), command,
                                       quoteShellPath(stdoutFile), quoteShellPath(stderrFile));
#endif

    const int systemStatus = system(finalCommand.c_str());
    OutputAndStatus result;
#ifdef _WIN32
    result.exitStatus = systemStatus == -1 ? EXIT_FAILURE : systemStatus;
#else
    if (systemStatus == -1)
    {
        result.exitStatus = EXIT_FAILURE;
    }
    else if (WIFEXITED(systemStatus))
    {
        result.exitStatus = WEXITSTATUS(systemStatus);
    }
    else if (WIFSIGNALED(systemStatus))
    {
        result.exitStatus = 128 + WTERMSIG(systemStatus);
    }
#endif

    if (std::filesystem::is_regular_file(stdoutFile, error))
    {
        result.output = fileToString(stdoutFile.string());
    }
    error.clear();
    string errorOutput;
    if (std::filesystem::is_regular_file(stderrFile, error))
    {
        errorOutput = fileToString(stderrFile.string());
    }
    if (!result.output.empty() && !errorOutput.empty())
    {
        result.output += "\n--- STDERR ---\n";
    }
    result.output += errorOutput;

    error.clear();
    std::filesystem::remove(stdoutFile, error);
    error.clear();
    std::filesystem::remove(stderrFile, error);
    return result;
}

void RunCommand::reset()
{
    releaseOutput();
    readPipe = invalidHandle;
    writePipe = invalidHandle;
    pid = invalidHandle;
    exitStatus = EXIT_FAILURE;
    haveWritePipe = false;
#ifdef _WIN32
    index = invalidHandle;
    readPending = false;
    pipeEof = false;
#endif
}

#ifdef _WIN32

// Partially adapted from Ninja's process management approach.
uint64_t RunCommand::startAsyncProcess(char *command, Builder &builder, BTarget *bTarget, const bool haveWritePipe_)
{
    haveWritePipe = haveWritePipe_;

    static uint64_t pipeInvocation = 0;
    const string readPipeName = FORMAT("\\\\.\\pipe\\hmake-{}-{}", GetCurrentProcessId(), pipeInvocation++);

    HANDLE readPipeHandle = CreateNamedPipeA(readPipeName.c_str(),
                                             PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                             PIPE_TYPE_BYTE | PIPE_REJECT_REMOTE_CLIENTS, 1, 0, 0, INFINITE, nullptr);
    if (readPipeHandle == INVALID_HANDLE_VALUE)
    {
        printErrorMessage(FORMAT("Could not create the child-process IPC pipe.\nPipe name: {}\n"
                                 "Operation: CreateNamedPipeA\nSystem error: {}",
                                 readPipeName, P2978::getErrorString()));
    }

    OVERLAPPED connectionOperation{};
    const BOOL connectedSynchronously = ConnectNamedPipe(readPipeHandle, &connectionOperation);
    const DWORD connectionError = connectedSynchronously ? ERROR_SUCCESS : GetLastError();
    if (!connectedSynchronously && connectionError != ERROR_IO_PENDING && connectionError != ERROR_PIPE_CONNECTED)
    {
        printErrorMessage(FORMAT("Could not connect the child-process IPC pipe.\nPipe name: {}\n"
                                 "Operation: ConnectNamedPipe\nSystem error: {}",
                                 readPipeName, P2978::getErrorString()));
    }

    SECURITY_ATTRIBUTES inheritableAttributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE childPipe = CreateFileA(readPipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, &inheritableAttributes,
                                   OPEN_EXISTING, 0, nullptr);
    if (childPipe == INVALID_HANDLE_VALUE)
    {
        printErrorMessage(FORMAT("Could not open the child-process side of the IPC pipe.\nPipe name: {}\n"
                                 "Operation: CreateFileA\nSystem error: {}",
                                 readPipeName, P2978::getErrorString()));
    }

    if (connectionError == ERROR_IO_PENDING)
    {
        DWORD ignoredBytes = 0;
        if (!GetOverlappedResult(readPipeHandle, &connectionOperation, &ignoredBytes, TRUE))
        {
            printErrorMessage(FORMAT("Could not complete the child-process IPC pipe connection.\nPipe name: {}\n"
                                     "Operation: GetOverlappedResult\nSystem error: {}",
                                     readPipeName, P2978::getErrorString()));
        }
    }
    // Associate only after ConnectNamedPipe has completed. Otherwise its zero-byte completion is indistinguishable
    // from a data-read completion and the same OVERLAPPED storage cannot safely be reused for ReadFile.
    index = builder.registerEventData(bTarget, (uint64_t)readPipeHandle);
    if (!CreateIoCompletionPort(readPipeHandle, (HANDLE)builder.serverFd, index, 0))
    {
        printErrorMessage(FORMAT("Could not attach the child-process pipe to the build event loop.\nPipe name: {}\n"
                                 "Operation: CreateIoCompletionPort\nSystem error: {}",
                                 readPipeName, P2978::getErrorString()));
    }
    HANDLE childInput = childPipe;
    if (!haveWritePipe)
    {
        childInput = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritableAttributes,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (childInput == INVALID_HANDLE_VALUE)
        {
            printErrorMessage(
                FORMAT("Could not open NUL for child-process input.\nSystem error: {}", P2978::getErrorString()));
        }
    }

    STARTUPINFOEXA startupInfo{};
    startupInfo.StartupInfo.cb = sizeof(startupInfo);
    startupInfo.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.StartupInfo.hStdInput = childInput;
    startupInfo.StartupInfo.hStdOutput = childPipe;
    startupInfo.StartupInfo.hStdError = childPipe;

    SIZE_T attributeBytes = 0;
    InitializeProcThreadAttributeList(nullptr, 2, 0, &attributeBytes);
    static std::vector<unsigned char> attributeStorage;
    if (attributeStorage.size() < attributeBytes)
    {
        attributeStorage.resize(attributeBytes);
    }
    startupInfo.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
    if (!InitializeProcThreadAttributeList(startupInfo.lpAttributeList, 2, 0, &attributeBytes))
    {
        printErrorMessage(
            FORMAT("Could not initialize the child-process handle list.\nSystem error: {}", P2978::getErrorString()));
    }
    HANDLE inheritedHandles[2] = {childPipe, childInput};
    const SIZE_T inheritedHandleCount = haveWritePipe ? 1 : 2;
    if (!UpdateProcThreadAttribute(startupInfo.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inheritedHandles,
                                   sizeof(HANDLE) * inheritedHandleCount, nullptr, nullptr))
    {
        printErrorMessage(
            FORMAT("Could not restrict inherited child-process handles.\nSystem error: {}", P2978::getErrorString()));
    }
    HANDLE childProcessJob = getChildProcessJob();
    if (!UpdateProcThreadAttribute(startupInfo.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, &childProcessJob,
                                   sizeof(childProcessJob), nullptr, nullptr))
    {
        printErrorMessage(FORMAT("Could not attach parent-death handling to the child process.\nSystem error: {}",
                                 P2978::getErrorString()));
    }

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    // Ctrl+C is handled outside console-pool subprocesses.
    const DWORD processFlags = CREATE_NEW_PROCESS_GROUP | EXTENDED_STARTUPINFO_PRESENT;

    // Do not prepend "cmd /c" on Windows; it breaks long command lines (>8191 chars).
    if (!CreateProcessA(nullptr, command, nullptr, nullptr,
                        /* inherit handles */ TRUE, processFlags, nullptr, nullptr, &startupInfo.StartupInfo,
                        &process_info))
    {
        printErrorMessage(FORMAT("Could not create the asynchronous child process.\nCommand: {}\n"
                                 "Operation: CreateProcessA\nSystem error: {}",
                                 command, P2978::getErrorString()));
    }

    DeleteProcThreadAttributeList(startupInfo.lpAttributeList);

    if (!CloseHandle(childPipe))
    {
        printErrorMessage(FORMAT("Could not close the inherited child-process pipe handle.\nSystem error: {}",
                                 P2978::getErrorString()));
    }
    if (!haveWritePipe && !CloseHandle(childInput))
    {
        printErrorMessage(
            FORMAT("Could not close the child-process NUL handle.\nSystem error: {}", P2978::getErrorString()));
    }
    if (!CloseHandle(process_info.hThread))
    {
        printErrorMessage(
            FORMAT("Could not close the child-process thread handle.\nSystem error: {}", P2978::getErrorString()));
    }
    readPipe = (uint64_t)readPipeHandle;
    writePipe = haveWritePipe ? (uint64_t)readPipeHandle : -1;
    pid = (uint64_t)process_info.hProcess;

    ++builder.simultaneousProcessCount;
    acquireOutput();
    output->reserve(4096);
    startRead();
    return index;
}

void RunCommand::startRead()
{
    if (pipeEof)
    {
        return;
    }
    assert(!readPending);

    CompletionKey &k = eventData[index];
    k.readOverlapped = {};

    const uint64_t offset = output->size();
    output->resize(offset + 4096);

    readPending = true;
    const BOOL result = ReadFile((HANDLE)k.handle, output->data() + offset, 4096, nullptr, &k.readOverlapped);

    if (result)
    {
        // Even if the read completed synchronously, consume the stale completion event.
        return;
    }
    const DWORD error = GetLastError();
    if (error == ERROR_IO_PENDING)
    {
        return;
    }
    if (error == ERROR_BROKEN_PIPE)
    {
        readPending = false;
        pipeEof = true;
        output->resize(offset);
        // No kernel completion is queued for an immediate failure. Builder checks pipeEof after isEventCompleted,
        // isEventRegistered, or the incomplete-read path returns, avoiding an artificial IOCP round trip.
        return;
    }

    printErrorMessage(FORMAT("Could not start reading child-process output.\nEvent index: {}\n"
                             "Operation: ReadFile\nSystem error: {}",
                             index, P2978::getErrorString()));
}

CompleteReadType RunCommand::completeRead()
{
    readPending = false;
    CompletionKey &k = eventData[index];
    DWORD bytesRead = 0;
    if (!GetOverlappedResult((HANDLE)k.handle, &k.readOverlapped, &bytesRead, false))
    {
        if (GetLastError() == ERROR_BROKEN_PIPE)
        {
            output->resize(output->size() - (4096 - bytesRead)); // trim unused tail
            pipeEof = true;
            return CompleteReadType::COMPLETE_PROCESS;
        }
        printErrorMessage(FORMAT("Could not complete the child-process output read.\nEvent index: {}\n"
                                 "Operation: GetOverlappedResult\nSystem error: {}",
                                 index, P2978::getErrorString()));
    }
    output->resize(output->size() - (4096 - bytesRead)); // trim unused tail

    if (!bytesRead)
    {
        pipeEof = true;
        return CompleteReadType::COMPLETE_PROCESS;
    }

    if (haveWritePipe && output->ends_with(P2978::delimiter))
    {
        return CompleteReadType::COMPLETE_MESSAGE;
    }

    return CompleteReadType::INCOMPLETE;
}

void RunCommand::writeNoReadExpected(const string_view buffer)
{
    assert(haveWritePipe && writePipe != invalidHandle);

    HANDLE completionEvent = getWriteCompletionEvent();
    uint64_t totalWritten = 0;
    while (totalWritten != buffer.size())
    {
        const uint64_t remaining = buffer.size() - totalWritten;
        const DWORD chunkSize =
            static_cast<DWORD>(std::min<uint64_t>(remaining, std::numeric_limits<DWORD>::max()));
        OVERLAPPED operation{};
        // Suppress a write packet on the pipe's IOCP. Builder invokes this API only from its single scheduler thread,
        // so one process-lifetime completion event can be safely reused after every write has fully completed;
        // WriteFile resets that event when it submits the next operation.
        operation.hEvent = reinterpret_cast<HANDLE>(reinterpret_cast<ULONG_PTR>(completionEvent) | 1);

        DWORD bytesWritten = 0;
        BOOL succeeded = WriteFile(reinterpret_cast<HANDLE>(writePipe), buffer.data() + totalWritten, chunkSize,
                                   &bytesWritten, &operation);
        if (!succeeded && GetLastError() == ERROR_IO_PENDING)
        {
            succeeded = GetOverlappedResult(reinterpret_cast<HANDLE>(writePipe), &operation, &bytesWritten, TRUE);
        }
        if (!succeeded)
        {
            // IPCManagerCompiler writes a request and immediately blocks reading its response. A closed input pipe
            // therefore violates the request/response contract; surface the error instead of assuming that the
            // child's output pipe and process have also completed.
            printErrorMessage(FORMAT("Could not write the child-process IPC response.\nPipe handle: {}\n"
                                     "System error: {}",
                                     writePipe, P2978::getErrorString()));
        }
        if (!bytesWritten)
        {
            printErrorMessage(FORMAT("Could not complete the child-process IPC response.\nPipe handle: {}\n"
                                     "WriteFile completed without writing data.",
                                     writePipe));
        }
        totalWritten += bytesWritten;
    }
}

void RunCommand::writeReadExpected(const string_view buffer)
{
    writeNoReadExpected(buffer);
    startRead();
}

void RunCommand::reapProcess(Builder &builder)
{
    // Pipe EOF is the process protocol's termination signal: the child keeps stdout/stderr open until it exits.
    // This wait is therefore normally already satisfied and avoids a registered wait plus another IOCP round trip.
    if (WaitForSingleObject(reinterpret_cast<HANDLE>(pid), INFINITE) != WAIT_OBJECT_0)
    {
        printErrorMessage(FORMAT("Could not wait for the child process.\nProcess handle: {}\n"
                                 "Operation: WaitForSingleObject\nSystem error: {}",
                                 pid, P2978::getErrorString()));
    }

    DWORD processExitStatus = EXIT_FAILURE;
    if (!GetExitCodeProcess((HANDLE)pid, &processExitStatus))
    {
        printErrorMessage(FORMAT("Could not read the child-process exit code.\nProcess handle: {}\n"
                                 "Operation: GetExitCodeProcess\nSystem error: {}",
                                 pid, P2978::getErrorString()));
    }
    exitStatus = static_cast<int>(processExitStatus);

    if (!CloseHandle((HANDLE)pid))
    {
        printErrorMessage(FORMAT("Could not close the child-process handle.\nProcess handle: {}\nSystem error: {}", pid,
                                 P2978::getErrorString()));
    }
    if (!CloseHandle((HANDLE)readPipe))
    {
        printErrorMessage(FORMAT("Could not close the child-process pipe.\nPipe handle: {}\nSystem error: {}", readPipe,
                                 P2978::getErrorString()));
    }
    --builder.simultaneousProcessCount;
}

void RunCommand::killModuleProcess(Builder &builder)
{
    constexpr DWORD terminatedExitCode = 1;

    const DWORD initialWait = WaitForSingleObject((HANDLE)pid, 0);
    if (initialWait == WAIT_FAILED)
    {
        printErrorMessage(FORMAT("Could not query the module process before termination.\nProcess handle: {}\n"
                                 "System error: {}",
                                 pid, P2978::getErrorString()));
    }
    if (initialWait == WAIT_TIMEOUT && !TerminateProcess((HANDLE)pid, terminatedExitCode))
    {
        printErrorMessage(FORMAT("Could not terminate the module process.\nProcess handle: {}\nExit code: {}\n"
                                 "System error: {}",
                                 pid, terminatedExitCode, P2978::getErrorString()));
    }
    if (WaitForSingleObject((HANDLE)pid, INFINITE) != WAIT_OBJECT_0)
    {
        printErrorMessage(FORMAT("Could not wait for the terminated module process.\nProcess handle: {}\n"
                                 "System error: {}",
                                 pid, P2978::getErrorString()));
    }
    if (!CloseHandle((HANDLE)pid))
    {
        printErrorMessage(FORMAT("Could not close the terminated module process handle.\nProcess handle: {}\n"
                                 "System error: {}",
                                 pid, P2978::getErrorString()));
    }
    if (!CloseHandle((HANDLE)readPipe))
    {
        printErrorMessage(FORMAT("Could not close the terminated module pipe.\nPipe handle: {}\nSystem error: {}",
                                 readPipe, P2978::getErrorString()));
    }
    --builder.simultaneousProcessCount;
    builder.unregisterEventDataAtIndex(index);
}

#else

// TODO: audit and account for all close() calls.

uint64_t RunCommand::startAsyncProcess(char *command, Builder &builder, BTarget *bTarget, const bool haveWritePipe_)
{
    haveWritePipe = haveWritePipe_;

    wordexp_t p;
    if (wordexp(command, &p, WRDE_NOCMD) != 0)
    {
        printErrorMessage(
            FORMAT("Could not parse the asynchronous command line.\nCommand: {}\nOperation: wordexp", command));
    }

    int stdoutPipesLocal[2];
    if (pipe2(stdoutPipesLocal, O_CLOEXEC) == -1)
    {
        printErrorMessage(FORMAT("Could not create the asynchronous process output pipe.\nCommand: {}\n"
                                 "System error: {}",
                                 command, P2978::getErrorString()));
    }
    readPipe = stdoutPipesLocal[0];

    int stdinPipesLocal[2];
    if (haveWritePipe)
    {
        if (pipe2(stdinPipesLocal, O_CLOEXEC) == -1)
        {
            printErrorMessage(FORMAT("Could not create the asynchronous process input pipe.\nCommand: {}\n"
                                     "System error: {}",
                                     command, P2978::getErrorString()));
        }
        writePipe = stdinPipesLocal[1];
    }
    int nullInput = -1;
    if (!haveWritePipe)
    {
        nullInput = open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (nullInput == -1)
        {
            printErrorMessage(FORMAT("Could not open /dev/null for child-process input.\nCommand: {}\n"
                                     "System error: {}",
                                     command, P2978::getErrorString()));
        }
    }

    sigset_t terminationSignals;
    sigemptyset(&terminationSignals);
    sigaddset(&terminationSignals, SIGINT);
    sigaddset(&terminationSignals, SIGTERM);
    struct sigaction defaultSigpipe{};
    defaultSigpipe.sa_handler = SIG_DFL;
    sigemptyset(&defaultSigpipe.sa_mask);
    const pid_t parentPid = getpid();

    pid = vfork(); // vfork is intentional here.
    if (pid == -1)
    {
        printErrorMessage(FORMAT("Could not create the asynchronous child process.\nCommand: {}\nOperation: vfork\n"
                                 "System error: {}",
                                 command, P2978::getErrorString()));
    }

    if (pid == 0)
    {
        // Child process: only async-signal-safe calls are allowed before exec.
        // The scheduler blocks termination signals so signalfd can consume them.
        // Children must not inherit that mask, and should die if the scheduler exits.
        if (sigprocmask(SIG_UNBLOCK, &terminationSignals, nullptr) == -1 ||
            sigaction(SIGPIPE, &defaultSigpipe, nullptr) == -1 || prctl(PR_SET_PDEATHSIG, SIGKILL) == -1 ||
            getppid() != parentPid || dup2(stdoutPipesLocal[1], STDOUT_FILENO) == -1 ||
            dup2(stdoutPipesLocal[1], STDERR_FILENO) == -1)
        {
            _exit(127);
        }

        if (haveWritePipe)
        {
            if (dup2(stdinPipesLocal[0], STDIN_FILENO) == -1)
            {
                _exit(127);
            }
        }
        else if (dup2(nullInput, STDIN_FILENO) == -1)
        {
            _exit(127);
        }

        close(stdoutPipesLocal[0]);
        close(stdoutPipesLocal[1]);
        if (haveWritePipe)
        {
            close(stdinPipesLocal[0]);
            close(stdinPipesLocal[1]);
        }
        else
        {
            close(nullInput);
        }

        execvp(p.we_wordv[0], p.we_wordv);
        _exit(127); // Must use _exit(), never exit().
    }

    // Parent process.
    close(stdoutPipesLocal[1]);
    wordfree(&p); // Safe here: child has already exec'd.
    builder.registerEventData(bTarget, readPipe);

    if (haveWritePipe)
    {
        close(stdinPipesLocal[0]);
    }
    else
    {
        close(nullInput);
    }

    ++builder.simultaneousProcessCount;
    acquireOutput();

    return readPipe;
}

void RunCommand::startRead()
{
    // The level-triggered epoll registration remains active between messages; no read syscall is armed on Linux.
}

CompleteReadType RunCommand::completeRead()
{
    int64_t readSize;
    constexpr uint64_t chunkSize = 4 * 1024;
    const uint64_t oldSize = output->size();

    output->resize(oldSize + chunkSize);

    do
    {
        readSize = read(readPipe, output->data() + oldSize, chunkSize);
    } while (readSize == -1 && errno == EINTR);

    // Trim the string back to the actual number of bytes read.
    if (readSize <= 0)
    {
        output->resize(oldSize);
    }
    else
    {
        output->resize(oldSize + readSize);
    }

    if (readSize == -1)
    {
        printErrorMessage(FORMAT("Could not read asynchronous child-process output.\nFile descriptor: {}\n"
                                 "System error: {}",
                                 readPipe, P2978::getErrorString()));
    }
    if (readSize == 0)
    {
        return CompleteReadType::COMPLETE_PROCESS;
    }
    if (haveWritePipe && output->ends_with(P2978::delimiter))
    {
        return CompleteReadType::COMPLETE_MESSAGE;
    }
    return CompleteReadType::INCOMPLETE;
}

void RunCommand::writeNoReadExpected(const string_view buffer)
{
    assert(haveWritePipe && writePipe != invalidHandle);

    uint64_t totalWritten = 0;
    while (totalWritten != buffer.size())
    {
        int64_t bytesWritten;
        do
        {
            bytesWritten =
                write(static_cast<int>(writePipe), buffer.data() + totalWritten, buffer.size() - totalWritten);
        } while (bytesWritten == -1 && errno == EINTR);

        if (bytesWritten == -1)
        {
            // Builder ignores SIGPIPE so EPIPE reaches this deliberate error path. IPCManagerCompiler writes its
            // request and immediately blocks reading this response; a closed stdin therefore violates the protocol.
            printErrorMessage(FORMAT("Could not write the child-process IPC response.\nFile descriptor: {}\n"
                                     "System error: {}",
                                     writePipe, P2978::getErrorString()));
        }
        if (!bytesWritten)
        {
            printErrorMessage(FORMAT("Could not complete the child-process IPC response.\nFile descriptor: {}\n"
                                     "write completed without writing data.",
                                     writePipe));
        }
        totalWritten += static_cast<uint64_t>(bytesWritten);
    }
}

void RunCommand::writeReadExpected(const string_view buffer)
{
    writeNoReadExpected(buffer);
    startRead();
}

void RunCommand::reapProcess(Builder &builder)
{
    --builder.simultaneousProcessCount;
    int status;
    if (waitpid(pid, &status, 0) < 0)
    {
        printErrorMessage(FORMAT("Could not reap the asynchronous child process.\nProcess ID: {}\n"
                                 "Operation: waitpid\nSystem error: {}",
                                 pid, P2978::getErrorString()));
    }
    if (close(readPipe) == -1)
    {
        printErrorMessage(FORMAT("Could not close the child-process output pipe.\nFile descriptor: {}\n"
                                 "System error: {}",
                                 readPipe, P2978::getErrorString()));
    }

    if (writePipe != invalidHandle)
    {
        if (close(writePipe) == -1)
        {
            printErrorMessage(FORMAT("Could not close the child-process input pipe.\nFile descriptor: {}\n"
                                     "System error: {}",
                                     writePipe, P2978::getErrorString()));
        }
    }

    if (WIFEXITED(status))
    {
        exitStatus = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status))
    {
        exitStatus = 128 + WTERMSIG(status);
    }
    else
    {
        exitStatus = EXIT_FAILURE;
    }
}

void RunCommand::killModuleProcess(Builder &builder)
{
    --builder.simultaneousProcessCount;
    if (kill(pid, SIGKILL) != 0)
    {
        printErrorMessage(FORMAT("Could not terminate the module process.\nProcess ID: {}\nSignal: SIGKILL\n"
                                 "System error: {}",
                                 pid, P2978::getErrorString()));
    }
    builder.unregisterEventDataAtIndex(readPipe);
    int status = 0;
    while (waitpid(pid, &status, 0) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }
        printErrorMessage(FORMAT("Could not reap the terminated module process.\nProcess ID: {}\n"
                                 "Operation: waitpid\nSystem error: {}",
                                 pid, P2978::getErrorString()));
    }
    if (close(readPipe) == -1)
    {
        printErrorMessage(FORMAT("Could not close the terminated module output pipe.\nFile descriptor: {}\n"
                                 "System error: {}",
                                 readPipe, P2978::getErrorString()));
    }
    if (writePipe != invalidHandle && close(writePipe) == -1)
    {
        printErrorMessage(FORMAT("Could not close the terminated module input pipe.\nFile descriptor: {}\n"
                                 "System error: {}",
                                 writePipe, P2978::getErrorString()));
    }
}

#endif

string RunCommand::pruneOutput()
{
    // Extract the serialized payload from the output buffer.
    assert(output != nullptr);

    const uint64_t outputSize = output->size();
    const uint64_t delimiterSize = strlen(P2978::delimiter);
    const uint64_t trailerSize = sizeof(uint32_t) + delimiterSize;
    if (outputSize < trailerSize)
    {
        printErrorMessage(FORMAT("Malformed child-process message: payload size is missing.\n"
                                 "Output size: {} bytes\nMinimum size: {} bytes",
                                 outputSize, trailerSize));
    }
    if (memcmp(output->data() + outputSize - delimiterSize, P2978::delimiter, delimiterSize) != 0)
    {
        printErrorMessage("Malformed child-process message: protocol delimiter is missing.");
    }

    uint32_t payloadSize = 0;
    const uint64_t payloadSizeOffset = outputSize - trailerSize;
    memcpy(&payloadSize, output->data() + payloadSizeOffset, sizeof(payloadSize));
    if (!payloadSize)
    {
        printErrorMessage("Malformed child-process message: protocol payloads must not be empty.");
    }
    if (static_cast<uint64_t>(payloadSize) > outputSize - trailerSize)
    {
        printErrorMessage(FORMAT("Malformed child-process message: declared payload exceeds available output.\n"
                                 "Output size: {} bytes\nPayload size: {} bytes\nProtocol overhead: {} bytes",
                                 outputSize, payloadSize, trailerSize));
    }
    const uint64_t framedSize = trailerSize + payloadSize;
    const char *payloadStart = output->data() + (outputSize - framedSize);

    // todo
    // will like to return a string_view. this could be a problem for lit testing where whole files will be received.
    string str{payloadStart, payloadSize};
    output->resize(outputSize - framedSize);

    return str;
}
