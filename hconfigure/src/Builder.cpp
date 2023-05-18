
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

Builder::Builder(unsigned short roundBegin, unsigned short roundEnd, vector<BTarget *> &preSortBTargets)
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
    }
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

    finalBTargets.clear();
    for (unsigned i = 0; i < sortedBTargets.size(); ++i)
    {
        sortedBTargets[i]->getRealBTarget(round).indexInTopologicalSort = i;
        sortedBTargets[i]->duringSort(*this, round);
    }
}

void Builder::launchThreadsAndUpdateBTargets()
{
    vector<thread *> threads;

    // TODO
    // Transition from one round to the next round can be more seamless. Instead of thread recreation and destruction
    // old threads should wait. one thread should call populateFinalBTargets and then next threads should start
    // execution. Also tarjannode sorting could be more parallel.
    finalBTargetsIterator = finalBTargets.begin();

    unsigned short launchThreads = settings.maximumBuildThreads;
    if (launchThreads)
    {
        while (threads.size() != 2)
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
    finalBTargetsSizeGoal = 0;
}

static mutex updateMutex;

std::condition_variable cond;

void Builder::addNewBTargetInFinalBTargets(BTarget *bTarget)
{
    {
        std::lock_guard<std::mutex> lk(updateMutex);
        finalBTargets.emplace_back(bTarget);
        ++finalBTargetsSizeGoal;
        if (finalBTargetsIterator == finalBTargets.end())
        {
            --finalBTargetsIterator;
        }
    }
    cond.notify_all();
}

void Builder::updateBTargets()
{
    BTarget *bTarget;
    RealBTarget *realBTarget;
    unsigned short additions = 0;

    std::unique_lock<std::mutex> lk(updateMutex);

    while (true)
    {
        while (true)
        {
            if (finalBTargetsIterator != finalBTargets.end())
            {
                bTarget = *finalBTargetsIterator;
                realBTarget = &(bTarget->getRealBTarget(round));
                ++finalBTargetsIterator;
                updateMutex.unlock();
                cond.notify_all();
                break;
            }
            else if (finalBTargets.size() == finalBTargetsSizeGoal)
            {
                return;
            }
            cond.wait(lk);
        }

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
                    finalBTargets.emplace_back(dependent);
                    if (finalBTargetsIterator == finalBTargets.end())
                    {
                        --finalBTargetsIterator;
                    }
                    ++additions;
                }
            }
        }
    }
}

bool Builder::lastBTarget()
{
    std::lock_guard<std::mutex> lk(updateMutex);
    return finalBTargetsSizeGoal == finalBTargets.size();
}
