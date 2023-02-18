#include "Builder.hpp"
#include "BasicTargets.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include "Utilities.hpp"
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <stack>
#include <thread>

using std::thread, std::mutex, std::make_unique, std::unique_ptr, std::ifstream, std::ofstream, std::stack,
    std::filesystem::current_path;

bool Builder::isCTargetInSelectedSubDirectory(const CTarget &cTarget)
{
    path targetPath = cTarget.getSubDirForTarget();
    for (; targetPath.root_path() != targetPath; targetPath = (targetPath / "..").lexically_normal())
    {
        std::error_code ec;
        if (equivalent(targetPath, current_path(), ec))
        {
            return true;
        }
    }
    return false;
}

Builder::Builder()
{
    vector<BTarget *> bTargets;
    for (CTarget *cTarget : targetPointers<CTarget>)
    {
        if (BTarget *bTarget = cTarget->getBTarget(); bTarget)
        {
            bTargets.emplace_back(bTarget);
            if (isCTargetInSelectedSubDirectory(*cTarget))
            {
                bTarget->selectiveBuild = true;
            }
        }
    }

    for (BTarget *bTarget : bTargets)
    {
        bTarget->initializeForBuild(*this);
    }
    round = 1;
    finalBTargetsSizeGoal = finalBTargets.size();
    launchThreadsAndUpdateBTargets();

    for (BTarget *bTarget : bTargets)
    {
        bTarget->populateSourceNodesAndRemoveUnReferencedHeaderUnits();
    }
    populateSetTarjanNodesBTargetsSMFiles();
    round = 0;
    populateFinalBTargets();
    launchThreadsAndUpdateBTargets();
}

void Builder::populateSetTarjanNodesBTargetsSMFiles()
{
    for (auto &[moduleScope, moduleScopeData] : moduleScopes)
    {
        for (SMFile *smFileConst : moduleScopeData.smFiles)
        {
            auto &smFile = const_cast<SMFile &>(*smFileConst);
            CppSourceTarget *cppSourceTarget = smFile.target;
            for (const Json &requireJson : smFileConst->requiresJson)
            {
                string requireLogicalName = requireJson.at("logical-name").get<string>();
                if (requireLogicalName == smFile.logicalName)
                {
                    print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                          "In Scope\n{}\nModule\n{}\n can not depend on itself.\n", smFile.node->filePath);
                    exit(EXIT_FAILURE);
                }
                if (requireJson.contains("lookup-method"))
                {
                    continue;
                }
                if (auto it = moduleScopeData.requirePaths.find(requireLogicalName);
                    it == moduleScopeData.requirePaths.end())
                {
                    print(stderr, fg(static_cast<fmt::color>(settings.pcSettings.toolErrorOutput)),
                          "No File Provides This {}.\n", requireLogicalName);
                    exit(EXIT_FAILURE);
                }
                else
                {
                    SMFile &smFileDep = *(const_cast<SMFile *>(it->second));
                    smFile.addDependency(smFileDep, 0);
                }
            }

            if (smFile.target->linkOrArchiveTarget)
            {
                smFile.target->linkOrArchiveTarget->addDependency(smFile, 0);
            }
            cppSourceTarget->addDependency(smFile, 0);
        }
    }
}

void Builder::populateFinalBTargets()
{
    TBT::tarjanNodes = &tarjanNodesBTargets;
    TBT::findSCCS();
    tarjanNodesBTargets.clear();
    TBT::checkForCycle();
    size_t needsUpdate = 0;

    vector<BTarget *> sortedBTargets = std::move(TBT::topologicalSort);
    for (auto it = sortedBTargets.rbegin(); it != sortedBTargets.rend(); ++it)
    {
        BTarget *bTarget = *it;
        if (bTarget->selectiveBuild)
        {
            for (BTarget *dependency : bTarget->getRealBTarget(round).dependencies)
            {
                dependency->selectiveBuild = true;
            }
        }
    }
    // for(auto it = )
    for (unsigned i = 0; i < sortedBTargets.size(); ++i)
    {
        BTarget *bTarget = sortedBTargets[i];
        RealBTarget &realBTarget = bTarget->getRealBTarget(round);
        realBTarget.indexInTopologicalSort = i;
        for (BTarget *dependency : realBTarget.dependencies)
        {
            realBTarget.allDependencies.emplace(dependency);
            for (BTarget *dep : dependency->getRealBTarget(round).allDependencies)
            {
                realBTarget.allDependencies.emplace(dep);
            }
        }
        bTarget->duringSort(*this, round);
        for (BTarget *dependent : realBTarget.dependents)
        {
            RealBTarget &dependentRealBTarget = dependent->getRealBTarget(round);
            if (realBTarget.fileStatus == FileStatus::UPDATED)
            {
                --(dependentRealBTarget.dependenciesSize);
            }
            else if (realBTarget.fileStatus == FileStatus::NEEDS_UPDATE)
            {
                dependentRealBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
            }
        }
        if (realBTarget.fileStatus == FileStatus::NEEDS_UPDATE)
        {
            ++needsUpdate;
            if (!realBTarget.dependenciesSize)
            {
                finalBTargets.emplace_back(bTarget);
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

void Builder::launchThreadsAndUpdateBTargets()
{
    vector<thread *> threads;

    // TODO
    // Instead of launching all threads, only the required amount should be
    // launched. Following should be helpful for this calculation in DSL.
    // https://cs.stackexchange.com/a/16829
    finalBTargetsIterator = finalBTargets.begin();
    unsigned short launchThreads = 12;
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
}

static mutex updateMutex;
static mutex printMutex;

std::condition_variable cond;

void Builder::addNewBTargetInFinalBTargets(BTarget *bTarget)
{
    std::lock_guard<std::mutex> lk(updateMutex);
    finalBTargets.emplace_back(bTarget);
    ++finalBTargetsSizeGoal;
    if (finalBTargetsIterator == finalBTargets.end())
    {
        --finalBTargetsIterator;
    }
    cond.notify_all();
}

void Builder::updateBTargets()
{
    std::unique_lock<std::mutex> lk(updateMutex);
    BTarget *bTarget;
    if (finalBTargets.size() == finalBTargetsSizeGoal && finalBTargetsIterator == finalBTargets.end())
    {
        return;
    }
    bool shouldWait;
    if (finalBTargetsIterator == finalBTargets.end())
    {
        shouldWait = true;
    }
    else
    {
        bTarget = *finalBTargetsIterator;
        ++finalBTargetsIterator;
        shouldWait = false;
        updateMutex.unlock();
    }
    while (true)
    {
        if (shouldWait)
        {
            cond.wait(lk, [&]() {
                return finalBTargetsIterator != finalBTargets.end() || finalBTargets.size() == finalBTargetsSizeGoal;
            });
            if (finalBTargets.size() == finalBTargetsSizeGoal)
            {
                return;
            }
            else
            {
                bTarget = *finalBTargetsIterator;
                ++finalBTargetsIterator;
            }
            updateMutex.unlock();
        }

        bTarget->updateBTarget(round, *this);

        printMutex.lock();
        bTarget->printMutexLockRoutine(round);
        printMutex.unlock();

        updateMutex.lock();
        for (BTarget *dependent : bTarget->getRealBTarget(round).dependents)
        {
            RealBTarget &dependentRealBTarget = dependent->getRealBTarget(round);
            --(dependentRealBTarget.dependenciesSize);
            if (!dependentRealBTarget.dependenciesSize && dependentRealBTarget.fileStatus == FileStatus::NEEDS_UPDATE)
            {
                finalBTargets.emplace_back(dependent);
                if (finalBTargetsIterator == finalBTargets.end())
                {
                    --finalBTargetsIterator;
                }
                cond.notify_all();
            }
        }
        if (finalBTargetsIterator == finalBTargets.end())
        {
            shouldWait = true;
        }
        else
        {
            bTarget = *finalBTargetsIterator;
            ++finalBTargetsIterator;
            shouldWait = false;
            updateMutex.unlock();
        }
    }
}