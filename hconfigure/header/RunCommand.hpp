
#ifndef HMAKE_RUNCOMMAND_HPP
#define HMAKE_RUNCOMMAND_HPP

#include "string"

using std::string;

struct RunCommand
{
    struct OutputAndStatus
    {
        string output;
        int exitStatus;
    };
#ifdef _WIN32
    void *stdout_read;
    void *hProcess;
    void *hThread;
#else
    int stdout_pipe[2];
    int stderr_pipe[2];
    int pid;
#endif

    // command is 3 parts. 1) tool path 2) command without output and error files 3) output and error files.
    // while print is 2 parts. 1) tool path and command without output and error files. 2) output and error files.
    explicit RunCommand();
    void startProcess(const string &command, bool isModuleProcess);
    OutputAndStatus endProcess(bool endModuleProcess) const;
    void killModuleProcess(const string &processName) const;
};

#endif // HMAKE_RUNCOMMAND_HPP
