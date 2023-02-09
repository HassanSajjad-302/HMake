#ifndef HMAKE_BUILDER_HPP
#define HMAKE_BUILDER_HPP

#include "BuildTools.hpp"
#include "ModuleScope.hpp"
#include "SMFile.hpp"
#include "Settings.hpp"
#include <string>

using std::string;

struct ModuleScopeData
{
    set<CppSourceTarget *> cppTargets;
    set<SMFile *> smFiles;
    set<SMFile> headerUnits;
    // Which application header unit directory come from which target
    map<const Node *, CppSourceTarget *> appHUDirTarget;
    map<string, SMFile *> requirePaths;
};

class Builder
{
    unsigned short round = 0;
    vector<CppSourceTarget *> sourceTargets;
    unsigned long finalBTargetsIndex = 0;
    unsigned int finalBTargetsSizeGoal;

  public:
    vector<BTarget *> filteredBTargets;
    vector<BTarget *> finalBTargets;
    map<const ModuleScope *, ModuleScopeData> moduleScopes;
    vector<SMFile *> canBeCompiledModule;
    vector<CppSourceTarget *> canBeLinked;
    explicit Builder();
    void populateRequirePaths();
    void populateSetTarjanNodesBTargetsSMFiles();
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
