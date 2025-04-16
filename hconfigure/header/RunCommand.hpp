
#ifndef HMAKE_RUNCOMMAND_HPP
#define HMAKE_RUNCOMMAND_HPP

#ifdef USE_HEADER_UNITS
import "Settings.hpp";
#else
#include "Settings.hpp"
#endif

// Maybe use CRTP and inherit both SourceNode and LOAT from it. exitStatus is being copied currently.
struct RunCommand
{
    string printCommand;
    string commandOutput;
    int exitStatus;

    // command is 3 parts. 1) tool path 2) command without output and error files 3) output and error files.
    // while print is 2 parts. 1) tool path and command without output and error files. 2) output and error files.
    RunCommand(path toolPath, const string &runCommand, string printCommand_, bool isTarget_);
    void executePrintRoutine(uint32_t color, bool printOnlyOnError, Value sourceJson, uint64_t _index0 = UINT64_MAX,
                             uint64_t _index1 = UINT64_MAX, uint64_t _index2 = UINT64_MAX,
                             uint64_t _index3 = UINT64_MAX, uint64_t _index4 = UINT64_MAX) const;
    void executePrintRoutineRoundOne(struct SMFile const &smFile) const;
};

class CppSourceTarget;
struct PostCompile : RunCommand
{
    CppSourceTarget &target;

    explicit PostCompile(const CppSourceTarget &target_, const path &toolPath, const string &commandFirstHalf,
                         string printCommandFirstHalf);

    bool ignoreHeaderFile(string_view child) const;
    void parseDepsFromMSVCTextOutput(struct SourceNode &sourceNode, string &output) const;
    void parseDepsFromGCCDepsOutput(SourceNode &sourceNode) const;
    void parseHeaderDeps(SourceNode &sourceNode);
};

#endif // HMAKE_RUNCOMMAND_HPP
