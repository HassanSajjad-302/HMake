#ifndef HMAKE_BUILDER_HPP
#define HMAKE_BUILDER_HPP

#include "BuildTools.hpp"
#include "SMFile.hpp"
#include "Settings.hpp"
#include <string>

using std::string;

struct ModuleScope
{
    set<SMFile *, SMFilePointerComparator> smFiles;
    set<SMFile, SMFilePathAndVariantPathComparator> headerUnits;
    // Which application header unit directory come from which target
    map<const Node *, CppSourceTarget *> appHUDirTarget;
};

class Builder
{
    vector<CppSourceTarget *> sourceTargets;
    unsigned long finalBTargetsIndex = 0;
    unsigned int finalBTargetsSizeGoal;

  public:
    vector<BTarget *> filteredBTargets;
    vector<BTarget *> finalBTargets;
    map<const string *, ModuleScope> moduleScopes;
    vector<SMFile *> canBeCompiledModule;
    vector<CppSourceTarget *> canBeLinked;
    explicit Builder();
    void populateRequirePaths(map<SMFileVariantPathAndLogicalName, SMFile *, SMFileCompareLogicalName> &requirePaths);
    void populateSetTarjanNodesBTargetsSMFiles(
        const map<SMFileVariantPathAndLogicalName, SMFile *, SMFileCompareLogicalName> &requirePaths);
    void populateFinalBTargets();
    static set<string> getTargetFilePathsFromVariantFile(const string &fileName);
    static set<string> getTargetFilePathsFromProjectOrPackageFile(const string &fileName, bool isPackage);

    // This function is executed by multiple threads and is executed recursively until build is finished.
    void launchThreadsAndUpdateBTargets();
    void printDebugFinalBTargets();
    void updateBTargets();
    static bool isSubDirPathStandard(const path &headerUnitPath, set<string> &environmentIncludes);
    static bool isSubDirPathApplication(const path &headerUnitPath,
                                        map<string *, CppSourceTarget *> &applicationIncludes);
};

#endif // HMAKE_BUILDER_HPP
