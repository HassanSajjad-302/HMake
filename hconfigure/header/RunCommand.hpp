
#ifndef HMAKE_RUNCOMMAND_HPP
#define HMAKE_RUNCOMMAND_HPP

#ifdef USE_HEADER_UNITS
import "string";
#else
#include "string"
#endif

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
#endif

    // command is 3 parts. 1) tool path 2) command without output and error files 3) output and error files.
    // while print is 2 parts. 1) tool path and command without output and error files. 2) output and error files.
    explicit RunCommand(const string &runCommand);
    void startProcess(const string &command);
    OutputAndStatus endProcess() const;
};

#endif // HMAKE_RUNCOMMAND_HPP
