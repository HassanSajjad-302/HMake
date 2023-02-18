#ifndef HMAKE_BUILDER_HPP
#define HMAKE_BUILDER_HPP

#include "BuildTools.hpp"
#include "SMFile.hpp"
#include "Settings.hpp"
#include <list>
#include <string>

using std::string, std::list;

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
    list<BTarget *>::iterator finalBTargetsIterator;
    size_t finalBTargetsSizeGoal;

    static bool isCTargetInSelectedSubDirectory(const CTarget &cTarget);

  public:
    vector<BTarget *> filteredBTargets;
    list<BTarget *> finalBTargets;
    map<const CppSourceTarget *, ModuleScopeData> moduleScopes;
    vector<SMFile *> canBeCompiledModule;
    vector<CppSourceTarget *> canBeLinked;
    explicit Builder();
    void populateSetTarjanNodesBTargetsSMFiles();
    void populateFinalBTargets();
    static set<string> getTargetFilePathsFromVariantFile(const string &fileName);
    static set<string> getTargetFilePathsFromProjectOrPackageFile(const string &fileName, bool isPackage);

    // This function is executed by multiple threads and is executed recursively until build is finished.
    void launchThreadsAndUpdateBTargets();
    void addNewBTargetInFinalBTargets(BTarget *bTarget);
    void updateBTargets();
};

#endif // HMAKE_BUILDER_HPP
