
#ifndef HMAKE_RUNCOMMAND_HPP
#define HMAKE_RUNCOMMAND_HPP

#include "string"
#include <cstdint>

using std::string;

enum class ProcessState
{
    LAUNCHED,
    OUTPUT_CONNECTED,
    COMPLETED,
    CONNECTED,
    IPCFD_CLOSED,
    OUTPUTFD_CLOSED,
};

struct ReadDataInfo
{
    string message;
    bool completed;
    ReadDataInfo(string message_, bool completed_);
};

struct RunCommand
{
    string output;
    uint64_t readPipe;
    uint64_t writePipe;
    uint64_t pid;
    int exitStatus;
#ifdef _WIN32
    ProcessState processState = ProcessState::LAUNCHED;
#else
    ProcessState processState = ProcessState::OUTPUT_CONNECTED;
#endif

    // command is 3 parts. 1) tool path 2) command without output and error files 3) output and error files.
    // while print is 2 parts. 1) tool path and command without output and error files. 2) output and error files.
    RunCommand() = default;
    void runProcess(const char *command);

    uint64_t startAsyncProcess(const char *command, class Builder &builder, class BTarget *bTarget, bool haveWritePipe);
    ReadDataInfo isReadCompleted(uint64_t index);
    void reapProcess(bool haveWritePipe);
    void killModuleProcess(const string &processName) const;
    string pruneOutput();
};

#endif // HMAKE_RUNCOMMAND_HPP
