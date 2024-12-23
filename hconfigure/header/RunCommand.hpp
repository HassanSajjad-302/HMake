
#ifndef HMAKE_RUNCOMMAND_HPP
#define HMAKE_RUNCOMMAND_HPP

#ifdef USE_HEADER_UNITS
import "Settings.hpp";
#else
#include "Settings.hpp"
#endif

// Maybe use CRTP and inherit both SourceNode and LinkOrArchiveTarget from it. exitStatus is being copied currently.
struct RunCommand
{
    pstring printCommand;
    pstring commandOutput;
    int exitStatus;

    // command is 3 parts. 1) tool path 2) command without output and error files 3) output and error files.
    // while print is 2 parts. 1) tool path and command without output and error files. 2) output and error files.
    RunCommand(path toolPath, const pstring &runCommand, pstring printCommand_, bool isTarget_);
    void executePrintRoutine(uint32_t color, bool printOnlyOnError, PValue sourceJson, uint64_t _index0 = UINT64_MAX,
                             uint64_t _index1 = UINT64_MAX, uint64_t _index2 = UINT64_MAX,
                             uint64_t _index3 = UINT64_MAX, uint64_t _index4 = UINT64_MAX) const;
};

class CppSourceTarget;
struct PostCompile : RunCommand
{
    CppSourceTarget &target;

    explicit PostCompile(const CppSourceTarget &target_, const path &toolPath, const pstring &commandFirstHalf,
                         pstring printCommandFirstHalf);

    bool ignoreHeaderFile(pstring_view child) const;
    void parseDepsFromMSVCTextOutput(struct SourceNode &sourceNode, pstring &output, bool mustConsiderHeaderDeps) const;
    void parseDepsFromGCCDepsOutput(SourceNode &sourceNode, bool mustConsiderHeaderDeps) const;
    void parseHeaderDeps(SourceNode &sourceNode, bool mustConsiderHeaderDeps);
};

#endif // HMAKE_RUNCOMMAND_HPP
