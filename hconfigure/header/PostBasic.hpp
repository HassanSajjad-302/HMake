
#ifndef HMAKE_POSTBASIC_HPP
#define HMAKE_POSTBASIC_HPP

#ifdef USE_HEADER_UNITS
#include "BuildTools.hpp"
#include "Settings.hpp"
#else
#include "BuildTools.hpp"
#include "Settings.hpp"
#endif

// Maybe use CRTP and inherit both SourceNode and LinkOrArchiveTarget from it. exitStatus is being copied currently.
struct PostBasic
{
    string printCommand;
    string commandSuccessOutput;
    string commandErrorOutput;
    int exitStatus;

    /* Could be a target or a file. For target (link and archive), we add extra _t at the end of the target name.*/
    bool isTarget;

    // command is 3 parts. 1) tool path 2) command without output and error files 3) output and error files.
    // while print is 2 parts. 1) tool path and command without output and error files. 2) output and error files.
    explicit PostBasic(const BuildTool &buildTool, const string &commandFirstHalf, string printCommandFirstHalf,
                       const string &buildCacheFilesDirPath, const string &fileName, const PathPrint &pathPrint,
                       bool isTarget_);

    void executePrintRoutine(uint32_t color, bool printOnlyOnError) const;
};

class CppSourceTarget;
struct PostCompile : PostBasic
{
    CppSourceTarget &target;

    explicit PostCompile(const CppSourceTarget &target_, const BuildTool &buildTool, const string &commandFirstHalf,
                         string printCommandFirstHalf, const string &buildCacheFilesDirPath, const string &fileName,
                         const PathPrint &pathPrint);

    bool ignoreHeaderFile(const string &str);
    void parseDepsFromMSVCTextOutput(struct SourceNode &sourceNode, string &output);
    void parseDepsFromGCCDepsOutput(SourceNode &sourceNode);
    void parseHeaderDeps(SourceNode &sourceNode);
};

#endif // HMAKE_POSTBASIC_HPP
