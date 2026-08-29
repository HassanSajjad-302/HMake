
#ifndef HMAKE_RUNCOMMAND_HPP
#define HMAKE_RUNCOMMAND_HPP

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

using std::string, std::string_view;

enum class CompleteReadType
{
    INCOMPLETE,
    COMPLETE_PROCESS,
    COMPLETE_MESSAGE,
};

struct RunCommand
{
    struct OutputAndStatus
    {
        string output;
        int exitStatus = EXIT_FAILURE;
    };

    static constexpr uint64_t invalidHandle = -1;

    /// Leased from the process-wide output pool while an asynchronous result is live. Every non-null pointer is
    /// pool-owned; synchronous runs return their own string in OutputAndStatus.
    string *output = nullptr;
    uint64_t readPipe = invalidHandle;
    uint64_t writePipe = invalidHandle;
    uint64_t pid = invalidHandle;
    int exitStatus = EXIT_FAILURE;
    bool haveWritePipe = false;
#ifdef _WIN32
    uint64_t index = invalidHandle;
    bool readPending = false;
    bool pipeEof = false;
#endif

    RunCommand() = default;
    ~RunCommand();
    RunCommand(const RunCommand &) = delete;
    RunCommand &operator=(const RunCommand &) = delete;
    RunCommand(RunCommand &&) = delete;
    RunCommand &operator=(RunCommand &&) = delete;

    /// Runs a shell command synchronously with inherited stdin and separately captured stdout/stderr.
    /// This path does not use any instance or pooled asynchronous state. Call it from one thread at a time.
    /// An explicit working directory also owns the capture files; otherwise they use the OS temporary directory.
    [[nodiscard]] static OutputAndStatus runProcess(
        const char *command, const std::filesystem::path &workingDirectory = {});

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
