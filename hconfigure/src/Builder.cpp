
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

Builder::Builder(unsigned short roundBegin, unsigned short roundEnd, list<BTarget *> &preSortBTargets_)
    : preSortBTargets{preSortBTargets_}
{
    round = roundBegin;
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
            exit(EXIT_SUCCESS);
        }

        if (round == roundEnd)
        {
            breakLoop = true;
        }
        else
        {
            --round;
        }
    }
}

void Builder::populateFinalBTargets()
{
    auto &k = tarjanNodesBTargets.try_emplace(round, set<TBT>()).first->second;
    TBT::tarjanNodes = &(k);
    TBT::findSCCS();
    TBT::checkForCycle();
    size_t needsUpdate = 0;

    vector<BTarget *> sortedBTargets = std::move(TBT::topologicalSort);
    if (!round)
    {
        for (auto it = sortedBTargets.rbegin(); it != sortedBTargets.rend(); ++it)
        {
            BTarget *bTarget = *it;
            if (bTarget->selectiveBuild)
            {
                for (BTarget *dependency : bTarget->getRealBTarget(0).dependencies)
                {
                    dependency->selectiveBuild = true;
                }
            }
        }
    }

    finalBTargets.clear();
    for (unsigned i = 0; i < sortedBTargets.size(); ++i)
    {
        BTarget *bTarget = sortedBTargets[i];
        RealBTarget &realBTarget = bTarget->getRealBTarget(round);
        bTarget->duringSort(*this, round, i);
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
}

static mutex updateMutex;

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
        RealBTarget *realBTarget;

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
            realBTarget = &(bTarget->getRealBTarget(round));
            updateMutex.unlock();
        }

        if (realBTarget->exitStatus == EXIT_SUCCESS)
        {
            bTarget->updateBTarget(round, *this);
        }
        if (realBTarget->exitStatus != EXIT_SUCCESS)
        {
            updateBTargetFailed = true;
        }

        updateMutex.lock();

        for (BTarget *dependent : bTarget->getRealBTarget(round).dependents)
        {
            RealBTarget &dependentRealBTarget = dependent->getRealBTarget(round);
            if (realBTarget->exitStatus != EXIT_SUCCESS)
            {
                dependentRealBTarget.exitStatus = EXIT_FAILURE;
            }
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
