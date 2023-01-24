
#ifndef HMAKE_POSTBASIC_HPP
#define HMAKE_POSTBASIC_HPP
#include "BuildTools.hpp"
#include "Settings.hpp"
struct PostBasic
{
    string printCommand;
    string commandSuccessOutput;
    string commandErrorOutput;
    bool successfullyCompleted;

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

    bool checkIfFileIsInEnvironmentIncludes(const string &str);

    void parseDepsFromMSVCTextOutput(struct SourceNode &sourceNode);

    void parseDepsFromGCCDepsOutput(SourceNode &sourceNode);

    void executePostCompileRoutineWithoutMutex(SourceNode &sourceNode);
};

#endif // HMAKE_POSTBASIC_HPP
