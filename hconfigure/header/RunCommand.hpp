
#ifndef HMAKE_RUNCOMMAND_HPP
#define HMAKE_RUNCOMMAND_HPP

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

using std::string, std::string_view;

enum class ProcessState
{
    LAUNCHED,
    OUTPUT_CONNECTED,
    COMPLETED,
    CONNECTED,
    IPCFD_CLOSED,
    OUTPUTFD_CLOSED,
};

enum class CompleteReadType
{
    INCOMPLETE,
    COMPLETE_PROCESS,
    COMPLETE_MESSAGE,
};

struct RunCommand
{
    static constexpr uint64_t invalidHandle = static_cast<uint64_t>(-1);

    /// Leased from the process-wide output pool while a result is live. Every non-null pointer is pool-owned.
    string *output = nullptr;
    uint64_t readPipe = static_cast<uint64_t>(-1);
    uint64_t writePipe = static_cast<uint64_t>(-1);
    uint64_t pid = static_cast<uint64_t>(-1);
    int exitStatus = EXIT_FAILURE;
    bool haveWritePipe = false;
#ifdef _WIN32
    uint64_t index = static_cast<uint64_t>(-1);
    bool readPending = false;
    bool pipeEof = false;
#endif

    // command is 3 parts. 1) tool path 2) command without output and error files 3) output and error files.
    // while print is 2 parts. 1) tool path and command without output and error files. 2) output and error files.
    RunCommand() = default;
    ~RunCommand();
    RunCommand(const RunCommand &) = delete;
    RunCommand &operator=(const RunCommand &) = delete;
    RunCommand(RunCommand &&) = delete;
    RunCommand &operator=(RunCommand &&) = delete;

    void runProcess(const char *command);

    uint64_t startAsyncProcess(char *command, class Builder &builder, class BTarget *bTarget, bool haveWritePipe_);
    /// Restores the inactive default state and returns the output buffer to the pool. Call explicitly before reusing
    /// this object after an asynchronous process has terminated.
    void reset();

    /// Returns the current output buffer to the pool. Call only after its contents and all asynchronous reads are done.
    void releaseOutput();

    /// Sends a complete child-input message, then immediately starts reading the child's next response.
    void writeReadExpected(string_view buffer);
    /// Sends a complete child-input message without starting another read.
    void writeNoReadExpected(string_view buffer);

    /// Arms the next child-output read on Windows. Linux keeps its level-triggered epoll registration active.
    void startRead();
    CompleteReadType completeRead();
    void reapProcess(Builder &builder);
    /// Terminates and reaps the process without releasing its output or resetting the remaining state.
    void killModuleProcess(Builder &builder);
    string pruneOutput();

  private:
    void acquireOutput();
};

#endif // HMAKE_RUNCOMMAND_HPP
