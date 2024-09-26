
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
    pstring commandSuccessOutput;
    pstring commandErrorOutput;
    int exitStatus;

    /* Could be a target or a file. For target (link and archive), we add extra _t at the end of the target name.*/
    bool isTarget;

    // command is 3 parts. 1) tool path 2) command without output and error files 3) output and error files.
    // while print is 2 parts. 1) tool path and command without output and error files. 2) output and error files.
    RunCommand(path toolPath, const pstring &runCommand, pstring printCommand_, bool isTarget_);

    void executePrintRoutine(uint32_t color, bool printOnlyOnError) const;
};

class CppSourceTarget;
struct PostCompile : RunCommand
{
    CppSourceTarget &target;

    explicit PostCompile(const CppSourceTarget &target_, const path &toolPath, const pstring &commandFirstHalf,
                         pstring printCommandFirstHalf);

    bool ignoreHeaderFile(pstring_view child) const;
    void parseDepsFromMSVCTextOutput(struct SourceNode &sourceNode, pstring &output, PValue &headerDepsJson,
                                     bool mustConsiderHeaderDeps) const;
    void parseDepsFromGCCDepsOutput(SourceNode &sourceNode, PValue &headerDepsJson, bool mustConsiderHeaderDeps);
    void parseHeaderDeps(SourceNode &sourceNode, bool parseFromErrorOutput, bool mustConsiderHeaderDeps);
};

#endif // HMAKE_RUNCOMMAND_HPP
