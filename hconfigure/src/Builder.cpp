
#ifdef USE_HEADER_UNITS
import "Builder.hpp";
import "BasicTargets.hpp";
import "Utilities.hpp";
import <condition_variable>;
import <fstream>;
import <mutex>;
import <stack>;
import <thread>;
#else
#include "Builder.hpp"
#include "BasicTargets.hpp"
#include "Utilities.hpp"
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <stack>
#include <thread>
#endif
using std::thread, std::mutex, std::make_unique, std::unique_ptr, std::ifstream, std::ofstream, std::stack,
    std::filesystem::current_path;

Builder::Builder()
{
    round = 2;

    for (CTarget *cTarget : targetPointers<CTarget>)
    {
        if (cTarget->callPreSort)
        {
            if (BTarget *bTarget = cTarget->getBTarget(); bTarget)
            {
                preSortBTargets.emplace_back(bTarget);
                if (cTarget->getSelectiveBuild())
                {
                    bTarget->selectiveBuild = true;
                }
            }
        }
    }
    preSortBTargetsIterator = preSortBTargets.begin();

    vector<thread *> threads;

    updateBTargetsIterator = updateBTargets.begin();

    unsigned int launchThreads = 1;
    numberOfLaunchedThreads = launchThreads;
    if (launchThreads)
    {
        while (threads.size() != launchThreads - 1)
        {
            threads.emplace_back(new thread{&Builder::execute, this});
        }
        execute();
    }
    for (thread *t : threads)
    {
        t->join();
        delete t;
    }

    /*    updateBTargetsSizeGoal = 0;

        bool breakLoop = false;
        while (true)
        {
            if (breakLoop)
            {
                break;
            }

            // preSort is called only for few of BTarget. If preSort is needed then the bTarget can be added in the
            // preSortBTargets of builder.
            for (BTarget *bTarget : preSortBTargets)
            {
                bTarget->preSort(*this, round);
            }
            populateFinalBTargets();
            launchThreadsAndUpdateBTargets();

            if (updateBTargetFailed)
            {
                throw std::exception();
            }

            if (round == roundEnd)
            {
                breakLoop = true;
            }
            else
            {
                --round;
            }
        }*/
}

void Builder::populateFinalBTargets()
{
    auto &k = tarjanNodesBTargets.try_emplace(round, set<TBT>()).first->second;
    TBT::tarjanNodes = &(k);
    TBT::findSCCS();
    TBT::checkForCycle();

    vector<BTarget *> sortedBTargets = std::move(TBT::topologicalSort);
    if (!round)
    {
        for (auto it = sortedBTargets.rbegin(); it != sortedBTargets.rend(); ++it)
        {
            BTarget *bTarget = *it;
            if (bTarget->selectiveBuild)
            {
                for (auto &[dependency, bTargetDepType] : bTarget->getRealBTarget(0).dependencies)
                {
                    if (bTargetDepType == BTargetDepType::FULL)
                    {
                        dependency->selectiveBuild = true;
                    }
                }
            }
        }
    }

    // finalBTargets.clear();
    for (unsigned i = 0; i < sortedBTargets.size(); ++i)
    {
        sortedBTargets[i]->getRealBTarget(round).indexInTopologicalSort = i;
        sortedBTargets[i]->duringSort(*this, round);
    }
}

static mutex updateMutex;

std::condition_variable cond;

void Builder::addNewBTargetInFinalBTargets(BTarget *bTarget)
{
    {
        std::lock_guard<std::mutex> lk(updateMutex);
        updateBTargets.emplace_back(bTarget);
        ++updateBTargetsSizeGoal;
        if (updateBTargetsIterator == updateBTargets.end())
        {
            --updateBTargetsIterator;
        }
    }
    cond.notify_all();
}

void Builder::execute()
{
    BTarget *bTarget = nullptr;
    RealBTarget *realBTarget = nullptr;
    bool nextMode = false;

    std::unique_lock<std::mutex> lk(updateMutex);

    while (true)
    {
        bool shouldBreak = false;
        while (true)
        {

            switch (builderMode)
            {
            case BuilderMode::PRE_SORT: {
                if (preSortBTargetsIterator == preSortBTargets.end())
                {
                    ++threadCount;
                    if (threadCount == numberOfLaunchedThreads)
                    {
                        nextMode = true;
                        threadCount = 0;
                    }
                }
                else
                {
                    bTarget = *preSortBTargetsIterator;
                    ++preSortBTargetsIterator;
                    updateMutex.unlock();
                    cond.notify_all();
                    shouldBreak = true;
                }
            }
            break;
            case BuilderMode::DURING_SORT: {

                if (duringSortBTargetsIterator != duringSortBTargets.end())
                {
                    bTarget = *duringSortBTargetsIterator;
                    ++duringSortBTargetsIterator;
                    updateMutex.unlock();
                    cond.notify_all();
                    shouldBreak = true;
                }
                else if (duringSortBTargets.size() == duringSortBTargetsSizeGoal)
                {
                    ++threadCount;
                    if (threadCount == numberOfLaunchedThreads)
                    {
                        nextMode = true;
                        threadCount = 0;
                    }
                }
            }
            break;
            case BuilderMode::UPDATE_BTARGET: {
                if (updateBTargetsIterator != updateBTargets.end())
                {
                    bTarget = *updateBTargetsIterator;
                    realBTarget = &(bTarget->getRealBTarget(round));
                    ++updateBTargetsIterator;
                    updateMutex.unlock();
                    cond.notify_all();
                    shouldBreak = true;
                }
                else if (updateBTargets.size() == updateBTargetsSizeGoal)
                {
                    ++threadCount;
                    if (threadCount == numberOfLaunchedThreads)
                    {
                        nextMode = true;
                        threadCount = 0;
                    }
                }
            }
            break;
            }

            if (shouldBreak)
            {
                break;
            }

            if (nextMode)
            {
                nextMode = false;
                switch (builderMode)
                {
                case BuilderMode::PRE_SORT: {
                    auto &k = tarjanNodesBTargets.try_emplace(round, set<TBT>()).first->second;
                    TBT::tarjanNodes = &(k);
                    TBT::findSCCS();
                    TBT::checkForCycle();

                    duringSortBTargets.clear();
                    updateBTargets.clear();

                    if (!round)
                    {
                        for (auto it = TBT::topologicalSort.rbegin(); it != TBT::topologicalSort.rend(); ++it)
                        {
                            BTarget *bTarget_ = *it;
                            if (bTarget_->selectiveBuild)
                            {
                                for (auto &[dependency, bTargetDepType] : bTarget_->getRealBTarget(0).dependencies)
                                {
                                    if (bTargetDepType == BTargetDepType::FULL)
                                    {
                                        dependency->selectiveBuild = true;
                                    }
                                }
                            }
                        }
                    }

                    // Index is only needed in round zero. Perform only for round one.
                    for (unsigned i = 0; i < TBT::topologicalSort.size(); ++i)
                    {
                        BTarget *bTarget_ = TBT::topologicalSort[i];
                        RealBTarget &r = bTarget_->getRealBTarget(round);
                        r.indexInTopologicalSort = i;
                        r.dependenciesSize = r.dependencies.size();
                        if (!r.dependenciesSize)
                        {
                            duringSortBTargets.emplace_back(TBT::topologicalSort[i]);
                        }
                    }

                    duringSortBTargetsSizeGoal = TBT::topologicalSort.size();
                    duringSortBTargetsIterator = duringSortBTargets.begin();
                    updateBTargetsSizeGoal = 0;
                    builderMode = BuilderMode::DURING_SORT;
                }
                break;
                case BuilderMode::DURING_SORT: {
                    updateBTargetsIterator = updateBTargets.begin();
                    builderMode = BuilderMode::UPDATE_BTARGET;
                }
                break;
                case BuilderMode::UPDATE_BTARGET: {
                    if (round)
                    {
                        preSortBTargetsIterator = preSortBTargets.begin();
                        builderMode = BuilderMode::PRE_SORT;
                        --round;
                    }
                    else
                    {
                        return;
                    }
                }
                break;
                }
            }
            else
            {
                cond.wait(lk);
            }
        }

        switch (builderMode)
        {
        case BuilderMode::PRE_SORT: {
            bTarget->preSort(*this, round);
        }
        break;
        case BuilderMode::DURING_SORT: {
            bTarget->duringSort(*this, round);
            /*            for (auto &[dependent, bTargetDepType] : realBTarget.dependents)
                        {
                            RealBTarget &dependentRealBTarget = dependent->getRealBTarget(round);
                            if (realBTarget.fileStatus == FileStatus::UPDATED || bTargetDepType ==
               BTargetDepType::LOOSE)
                            {
                                --(dependentRealBTarget.dependenciesSize);
                            }
                            else if (realBTarget.fileStatus == FileStatus::NEEDS_UPDATE)
                            {
                                dependentRealBTarget.dependencyNeedsUpdate = true;
                                dependentRealBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
                            }
                        }*/

            for (auto &[dependent, bTargetDepType] : bTarget->getRealBTarget(round).dependents)
            {
                RealBTarget &dependentRealBTarget = dependent->getRealBTarget(round);
                if (bTargetDepType == BTargetDepType::FULL)
                {
                    --(dependentRealBTarget.dependenciesSize);
                    if (!dependentRealBTarget.dependenciesSize)
                    {
                        duringSortBTargets.emplace_back(dependent);
                        if (duringSortBTargetsIterator == duringSortBTargets.end())
                        {
                            --duringSortBTargetsIterator;
                        }
                    }
                }
            }

            if (realBTarget->fileStatus == FileStatus::NEEDS_UPDATE)
            {
                ++updateBTargetsSizeGoal;
                if (!realBTarget->dependenciesSize)
                {
                    updateBTargets.emplace_back(bTarget);
                }
            }
        }
        break;
        case BuilderMode::UPDATE_BTARGET: {
            try
            {
                bTarget->updateBTarget(*this, round);
                updateMutex.lock();
                realBTarget->updateCalled = true;
                if (realBTarget->exitStatus != EXIT_SUCCESS)
                {
                    updateBTargetFailed = true;
                }
            }
            catch (std::exception &ec)
            {
                updateMutex.lock();
                realBTarget->exitStatus = EXIT_FAILURE;
                string str(ec.what());
                if (!str.empty())
                {
                    printErrorMessage(str);
                }
                if (realBTarget->exitStatus != EXIT_SUCCESS)
                {
                    updateBTargetFailed = true;
                }
            }

            bTarget->getRealBTarget(round).fileStatus = FileStatus::UPDATED;
            for (auto &[dependent, bTargetDepType] : bTarget->getRealBTarget(round).dependents)
            {
                RealBTarget &dependentRealBTarget = dependent->getRealBTarget(round);
                if (bTargetDepType == BTargetDepType::FULL)
                {
                    if (realBTarget->exitStatus != EXIT_SUCCESS)
                    {
                        dependentRealBTarget.exitStatus = EXIT_FAILURE;
                    }
                    --(dependentRealBTarget.dependenciesSize);
                    if (!dependentRealBTarget.dependenciesSize &&
                        dependentRealBTarget.fileStatus == FileStatus::NEEDS_UPDATE)
                    {
                        updateBTargets.emplace_back(dependent);
                        if (updateBTargetsIterator == updateBTargets.end())
                        {
                            --updateBTargetsIterator;
                        }
                    }
                }
            }
        }
        break;
        }
    }
}

bool Builder::addCppSourceTargetsInFinalBTargets(set<CppSourceTarget *> &targets)
{
    {
        std::lock_guard<std::mutex> lk(updateMutex);
        for (CppSourceTarget *target : targets)
        {
            if (target->targetCacheChanged)
            {
                updateBTargets.emplace_back(target);
                ++updateBTargetsSizeGoal;
                if (updateBTargetsIterator == updateBTargets.end())
                {
                    --updateBTargetsIterator;
                }
            }
        }
    }
    cond.notify_all();
    return false;
}
