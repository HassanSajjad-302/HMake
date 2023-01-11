#ifndef HMAKE_BUILDER_HPP
#define HMAKE_BUILDER_HPP

#include "BuildTools.hpp"
#include "SMFile.hpp"
#include "Settings.hpp"
#include <string>
using std::string;

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

struct ModuleScope
{
    set<SMFile *, SMFilePointerComparator> smFiles;
    set<SMFile, SMFilePathAndVariantPathComparator> headerUnits;
    // Which application header unit directory come from which target
    map<const Node *, Target *> appHUDirTarget;
};

class Target;
struct PostCompile : PostBasic
{
    Target &target;

    explicit PostCompile(const Target &target_, const BuildTool &buildTool, const string &commandFirstHalf,
                         string printCommandFirstHalf, const string &buildCacheFilesDirPath, const string &fileName,
                         const PathPrint &pathPrint);

    bool checkIfFileIsInEnvironmentIncludes(const string &str);

    void parseDepsFromMSVCTextOutput(SourceNode &sourceNode);

    void parseDepsFromGCCDepsOutput(SourceNode &sourceNode);

    void executePostCompileRoutineWithoutMutex(SourceNode &sourceNode);
};

class Builder
{
    vector<Target *> sourceTargets;
    unsigned long finalBTargetsIndex = 0;
    unsigned int finalBTargetsSizeGoal;

  public:
    vector<BTarget *> filteredBTargets;
    vector<BTarget *> finalBTargets;
    map<const string *, ModuleScope> moduleScopes;
    vector<SMFile *> canBeCompiledModule;
    vector<Target *> canBeLinked;
    explicit Builder();
    void populateRequirePaths(map<SMFileVariantPathAndLogicalName, SMFile *, SMFileCompareLogicalName> &requirePaths);
    void checkForHeaderUnitsCache();
    void populateSetTarjanNodesBTargetsSMFiles(
        const map<SMFileVariantPathAndLogicalName, SMFile *, SMFileCompareLogicalName> &requirePaths);
    void populateFinalBTargets();
    static set<string> getTargetFilePathsFromVariantFile(const string &fileName);
    static set<string> getTargetFilePathsFromProjectOrPackageFile(const string &fileName, bool isPackage);

    // This function is executed by multiple threads and is executed recursively until build is finished.
    void launchThreadsAndUpdateBTargets();
    void printDebugFinalBTargets();
    void updateBTargets();
    void createHeaderUnits();
    static bool isSubDirPathStandard(const path &headerUnitPath, set<string> &environmentIncludes);
    static bool isSubDirPathApplication(const path &headerUnitPath, map<string *, Target *> &applicationIncludes);
};

#endif // HMAKE_BUILDER_HPP
