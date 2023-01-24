#include "Builder.hpp"
#include "BasicTargets.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include "Utilities.hpp"
#include <fstream>
#include <mutex>
#include <stack>
#include <thread>

using std::thread, std::mutex, std::make_unique, std::unique_ptr, std::ifstream, std::ofstream, std::stack;

Builder::Builder()
{
    map<SMFileVariantPathAndLogicalName, SMFile *, SMFileCompareLogicalName> requirePaths;

    vector<BTarget *> bTargets;
    for (auto &[first, cTarget] : cTargets)
    {
        if (BTarget *bTarget = cTarget->getBTarget(); bTarget)
        {
            bTargets.emplace_back(bTarget);
        }
    }

    for (BTarget *bTarget : bTargets)
    {
        bTarget->initializeForBuild();
        bTarget->checkForPreBuiltAndCacheDir();
        bTarget->parseModuleSourceFiles(*this);
    }
    finalBTargetsSizeGoal = finalBTargets.size();
    launchThreadsAndUpdateBTargets();
    for (BTarget *bTarget : bTargets)
    {
        bTarget->checkForHeaderUnitsCache();
    }
    for (BTarget *bTarget : bTargets)
    {
        bTarget->populateSetTarjanNodesSourceNodes(*this);
    }
    populateRequirePaths(requirePaths);
    for (BTarget *bTarget : bTargets)
    {
        bTarget->createHeaderUnits();
    }
    populateSetTarjanNodesBTargetsSMFiles(requirePaths);
    populateFinalBTargets();
    launchThreadsAndUpdateBTargets();
}

void Builder::populateRequirePaths(
    map<SMFileVariantPathAndLogicalName, SMFile *, SMFileCompareLogicalName> &requirePaths)
{
    for (const auto &it : moduleScopes)
    {
        for (const auto &smFileConst : it.second.smFiles)
        {
            auto &smFile = const_cast<SMFile &>(*smFileConst);

            string smFilePath =
                smFile.target->buildCacheFilesDirPath + path(smFile.node->filePath).filename().string() + ".smrules";
            Json smFileJson;
            ifstream(smFilePath) >> smFileJson;
            bool hasRequires = false;
            Json rule = smFileJson.at("rules").get<Json>()[0];
            if (rule.contains("provides"))
            {
                smFile.hasProvide = true;
                // There can be only one provides but can be multiple requires.
                smFile.logicalName = rule.at("provides").get<Json>()[0].at("logical-name").get<string>();
                if (auto [pos, ok] = requirePaths.emplace(
                        SMFileVariantPathAndLogicalName(smFile.logicalName, *(smFile.target->variantFilePath)),
                        &smFile);
                    !ok)
                {
                    const auto &[key, val] = *pos;
                    // TODO
                    // Mention the module scope too.
                    print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                          "Module {} Is Being Provided By 2 different files:\n1){}\n2){}\n", smFile.logicalName,
                          smFile.node->filePath, val->node->filePath);
                    exit(EXIT_FAILURE);
                }
            }
            smFile.requiresJson = std::move(rule.at("requires"));
        }
    }
}

void Builder::populateSetTarjanNodesBTargetsSMFiles(
    const map<SMFileVariantPathAndLogicalName, SMFile *, SMFileCompareLogicalName> &requirePaths)
{
    for (auto &[scopeStrPtr, moduleScope] : moduleScopes)
    {
        for (SMFile *smFileConst : moduleScope.smFiles)
        {
            auto &smFile = const_cast<SMFile &>(*smFileConst);
            // If following remains false then source is GlobalModuleFragment.
            bool hasLogicalNameRequireDependency = false;
            // If following is true then smFile is PartitionImplementation.
            bool hasPartitionExportDependency = false;
            for (const Json &json : smFileConst->requiresJson)
            {
                string requireLogicalName = json.at("logical-name").get<string>();
                if (requireLogicalName == smFile.logicalName)
                {
                    print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                          "In Scope\n{}\nModule\n{}\n can not depend on itself.\n", smFile.node->filePath);
                    exit(EXIT_FAILURE);
                }
                if (json.contains("lookup-method"))
                {
                    string headerUnitPath =
                        path(json.at("source-path").get<string>()).lexically_normal().generic_string();
                    bool isStandardHeaderUnit = false;
                    bool isApplicationHeaderUnit = false;
                    CppSourceTarget *ahuDirTarget;
                    auto isSubDir = [](path p, const path &root) {
                        while (p != p.root_path())
                        {
                            if (p == root.parent_path())
                            {
                                return true;
                            }
                            p = p.parent_path();
                        }
                        return false;
                    };
                    // TODO
                    /*                    if (isSubDirPathStandard(headerUnitPath,
                       smFile.target.environment.includeDirectories))
                                        {
                                            isStandardHeaderUnit = true;
                                            ahuDirTarget = &(smFile.target);
                                        }*/
                    if (!isStandardHeaderUnit)
                    {
                        for (auto &dir : moduleScope.appHUDirTarget)
                        {
                            // TODO;
                            const Node *dirNode = dir.first;
                            path result = path(headerUnitPath).lexically_relative(dirNode->filePath).generic_string();
                            if (!result.empty() && !result.generic_string().starts_with(".."))
                            {
                                isApplicationHeaderUnit = true;
                                ahuDirTarget = dir.second;
                            }
                        }
                    }
                    if (!isStandardHeaderUnit && !isApplicationHeaderUnit)
                    {
                        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                              "Module Header Unit\n{}\n is neither a Standard Header Unit nor belongs to any Target "
                              "Header Unit Includes of Module Scope\n{}\n",
                              headerUnitPath, *scopeStrPtr);
                        exit(EXIT_FAILURE);
                    }
                    const auto &[pos1, Ok1] = moduleScope.headerUnits.emplace(ahuDirTarget, headerUnitPath);
                    auto &smFileHeaderUnit = const_cast<SMFile &>(*pos1);
                    smFile.headerUnitsConsumptionMethods[&smFileHeaderUnit].emplace(
                        json.at("lookup-method").get<string>() == "include-angle", requireLogicalName);
                    smFile.addDependency(smFileHeaderUnit);

                    if (Ok1)
                    {
                        smFileHeaderUnit.type = SM_FILE_TYPE::HEADER_UNIT;
                        smFileHeaderUnit.resultType = ResultType::CPP_MODULE;
                        smFileHeaderUnit.standardHeaderUnit = isStandardHeaderUnit;
                        if (isApplicationHeaderUnit)
                        {
                            smFileHeaderUnit.ahuTarget = ahuDirTarget;
                        }
                        // TODO:
                        // Check for update
                        smFileHeaderUnit.fileStatus = FileStatus::NEEDS_UPDATE;
                    }
                }
                else
                {
                    hasLogicalNameRequireDependency = true;
                    if (requireLogicalName.contains(':'))
                    {
                        hasPartitionExportDependency = true;
                    }
                    if (auto it = requirePaths.find(
                            SMFileVariantPathAndLogicalName(requireLogicalName, *(smFile.target->variantFilePath)));
                        it == requirePaths.end())
                    {
                        print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                              "No File Provides This {}.\n", requireLogicalName);
                        exit(EXIT_FAILURE);
                    }
                    else
                    {
                        SMFile &smFileDep = *(const_cast<SMFile *>(it->second));
                        smFile.addDependency(smFileDep);
                    }
                }
            }

            if (smFile.hasProvide)
            {
                smFile.type =
                    smFile.logicalName.contains(':') ? SM_FILE_TYPE::PARTITION_EXPORT : SM_FILE_TYPE::PRIMARY_EXPORT;
            }
            else
            {
                if (hasLogicalNameRequireDependency)
                {
                    smFile.type = hasPartitionExportDependency ? SM_FILE_TYPE::PARTITION_IMPLEMENTATION
                                                               : SM_FILE_TYPE::PRIMARY_IMPLEMENTATION;
                }
                else
                {
                    smFile.type = SM_FILE_TYPE::GLOBAL_MODULE_FRAGMENT;
                }
            }
            smFile.target->addDependency(smFile);
            smFile.target->smFiles.emplace_back(&smFile);
            smFile.resultType = ResultType::CPP_MODULE;
        }
    }
}

void Builder::populateFinalBTargets()
{
    TBT::tarjanNodes = &tarjanNodesBTargets;
    TBT::findSCCS();
    TBT::checkForCycle([](BTarget *target) -> string { return "cycle check function not implemented"; }); // TODO
    unsigned long needsUpdate = 0;
    for (unsigned long i = 0; i < TBT::topologicalSort.size(); ++i)
    {
        BTarget *bTarget = TBT::topologicalSort[i];
        bTarget->indexInTopologicalSort = i;
        if (bTarget->resultType == ResultType::LINK)
        {
            auto &target = static_cast<CppSourceTarget &>(*bTarget);
            auto b = target.bTargetType;
            if (target.fileStatus != FileStatus::NEEDS_UPDATE)
            {
                int a;
                // TODO
                /*                if (target.getLinkOrArchiveCommand() != target.linkCommand)
                                {
                                    target.fileStatus = FileStatus::NEEDS_UPDATE;
                                }*/
            }
        }
        for (BTarget *dependent : bTarget->dependents)
        {
            if (bTarget->fileStatus == FileStatus::UPDATED)
            {
                --(dependent->dependenciesSize);
            }
            else if (bTarget->fileStatus == FileStatus::NEEDS_UPDATE)
            {
                auto &dependentTarget = static_cast<CppSourceTarget &>(*dependent);
                dependentTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            }
        }
        if (bTarget->fileStatus == FileStatus::NEEDS_UPDATE)
        {
            ++needsUpdate;
            if (!bTarget->dependenciesSize)
            {
                finalBTargets.emplace_back(bTarget);
            }
        }
        for (BTarget *dependency : bTarget->dependencies)
        {
            bTarget->allDependencies.emplace(dependency);
            // Following assign all dependencies of the dependency to all dependencies of the bTarget. Because of
            // iteration in topologicalSort, indexInTopologicalSort for all dependencies would already be set.
            for (BTarget *dep : dependency->allDependencies)
            {
                bTarget->allDependencies.emplace(dep);
            }
        }
    }
    finalBTargetsSizeGoal = needsUpdate;
}

set<string> Builder::getTargetFilePathsFromVariantFile(const string &fileName)
{
    Json variantFileJson;
    ifstream(fileName) >> variantFileJson;
    return variantFileJson.at(JConsts::targets).get<set<string>>();
}

set<string> Builder::getTargetFilePathsFromProjectOrPackageFile(const string &fileName, bool isPackage)
{
    Json projectFileJson;
    ifstream(fileName) >> projectFileJson;
    vector<string> vec;
    if (isPackage)
    {
        vector<Json> pVariantJson = projectFileJson.at(JConsts::variants).get<vector<Json>>();
        for (const auto &i : pVariantJson)
        {
            vec.emplace_back(i.at(JConsts::index).get<string>());
        }
    }
    else
    {
        vec = projectFileJson.at(JConsts::variants).get<vector<string>>();
    }
    set<string> targetFilePaths;
    for (auto &i : vec)
    {
        for (const string &str :
             getTargetFilePathsFromVariantFile(i + (isPackage ? "/packageVariant.hmake" : "/projectVariant.hmake")))
        {
            targetFilePaths.emplace(str);
        }
    }
    return targetFilePaths;
}

mutex oneAndOnlyMutex;
mutex printMutex;

void Builder::launchThreadsAndUpdateBTargets()
{
    vector<thread *> threads;

    // TODO
    // Instead of launching all threads, only the required amount should be
    // launched. Following should be helpful for this calculation in DSL.
    // https://cs.stackexchange.com/a/16829
    unsigned short launchThreads = 1;
    if (launchThreads)
    {
        while (threads.size() != launchThreads - 1)
        {
            threads.emplace_back(new thread{&Builder::updateBTargets, this});
        }
        updateBTargets();
    }
    for (thread *t : threads)
    {
        t->join();
        delete t;
    }
    finalBTargets.clear();
    finalBTargetsIndex = 0;
}

void Builder::printDebugFinalBTargets()
{
    for (BTarget *bTarget : finalBTargets)
    {
    }
}

void Builder::updateBTargets()
{
    bool loop = true;
    while (loop)
    {
        oneAndOnlyMutex.lock();
        while (finalBTargetsIndex < finalBTargets.size())
        {
            printDebugFinalBTargets();
            BTarget *bTarget = finalBTargets[finalBTargetsIndex];
            ++finalBTargetsIndex;
            oneAndOnlyMutex.unlock();
            bTarget->updateBTarget();
            printMutex.lock();
            bTarget->printMutexLockRoutine();
            printMutex.unlock();
            oneAndOnlyMutex.lock();
            for (BTarget *dependent : bTarget->dependents)
            {
                --(dependent->dependenciesSize);
                if (!dependent->dependenciesSize && dependent->fileStatus == FileStatus::NEEDS_UPDATE)
                {
                    finalBTargets.emplace_back(dependent);
                }
            }
        }
        if (finalBTargetsIndex == finalBTargetsSizeGoal)
        {
            loop = false;
        }
        oneAndOnlyMutex.unlock();
    }
}

bool Builder::isSubDirPathStandard(const path &headerUnitPath, set<string> &environmentIncludes)
{
    string headerUnitPathSMallCase = headerUnitPath.generic_string();
    for (auto &c : headerUnitPathSMallCase)
    {
        c = (char)::tolower(c);
    }
    for (const string &str : environmentIncludes)
    {
        string directoryPathSmallCase = str;
        for (auto &c : directoryPathSmallCase)
        {
            c = (char)::tolower(c);
        }
        path result = path(headerUnitPathSMallCase).lexically_relative(directoryPathSmallCase).generic_string();
        if (!result.empty() && !result.generic_string().starts_with(".."))
        {
            return true;
        }
    }
    return false;
}

bool Builder::isSubDirPathApplication(const path &headerUnitPath, map<string *, CppSourceTarget *> &applicationIncludes)
{

    return false;
}