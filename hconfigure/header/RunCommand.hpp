
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

enum class CompleteReadType
{
    INCOMPLETE,
    COMPLETE_PROCESS,
    COMPLETE_MESSAGE,
};

struct RunCommand
{
    string *output = nullptr;
    uint64_t readPipe;
    uint64_t writePipe = -1;
    uint64_t pid = -1;
    int exitStatus;
#ifdef _WIN32
    CompleteReadType completeReadType = CompleteReadType::INCOMPLETE;
    uint64_t index;
#endif
    uint32_t outputIndex = 0;

    // command is 3 parts. 1) tool path 2) command without output and error files 3) output and error files.
    // while print is 2 parts. 1) tool path and command without output and error files. 2) output and error files.
    RunCommand() = default;
    void runProcess(const char *command);

    uint64_t startAsyncProcess(const char *command, class Builder &builder, class BTarget *bTarget, bool haveWritePipe);
    bool startRead();
    CompleteReadType completeRead();
    void reapProcess(Builder &builder);
    void killModuleProcess(Builder &builder) const;
    string pruneOutput();
};

#endif // HMAKE_RUNCOMMAND_HPP
