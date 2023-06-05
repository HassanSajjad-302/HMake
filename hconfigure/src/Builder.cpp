
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

    unsigned int launchThreads = 12;
    numberOfLaunchedThreads = launchThreads;
    while (threads.size() != launchThreads - 1)
    {
        threads.emplace_back(new thread{&Builder::execute, this});
    }
    execute();

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

extern string getThreadId();

#ifndef NDEBUG
unsigned short count = 0;
#endif

void Builder::execute()
{
    BTarget *bTarget = nullptr;
    RealBTarget *realBTarget = nullptr;

    // printMessage(fmt::format("Locking Update Mutex {}\n", __LINE__));
    std::unique_lock<std::mutex> lk(updateMutex);

    while (true)
    {
        while (true)
        {
            bool shouldBreak = false;
            bool nextMode = false;

            switch (builderMode)
            {
            case BuilderMode::PRE_SORT: {
                if (preSortBTargetsIterator == preSortBTargets.end())
                {
                    ++threadCount;
                    // printMessage(fmt::format("{} {} {} {} {}\n", round, "presort-exiting", threadCount,
                    //  numberOfLaunchedThreads, getThreadId()));
                    if (threadCount == numberOfLaunchedThreads)
                    {
                        threadCount = 0;
                        nextMode = true;

                        auto &k = tarjanNodesBTargets.try_emplace(round, set<TBT>()).first->second;
                        TBT::tarjanNodes = &(k);
                        TBT::findSCCS();
                        TBT::checkForCycle();

                        updateBTargets.clear();

                        size_t topSize = TBT::topologicalSort.size();

                        if (!round && topSize)
                        {
                            for (size_t i = TBT::topologicalSort.size(); i-- > 0;)
                            {
                                BTarget &localBTarget = *(TBT::topologicalSort[i]);
                                RealBTarget &localReal = localBTarget.getRealBTarget(0);
                                if (localBTarget.selectiveBuild)
                                {
                                    if (!localReal.dependenciesSize)
                                    {
                                        updateBTargets.emplace_front(&localBTarget);
                                    }
                                    localReal.indexInTopologicalSort = topSize - (i + 1);
                                    ++updateBTargetsSizeGoal;
                                    for (auto &[dependency, bTargetDepType] : localReal.dependencies)
                                    {
                                        if (bTargetDepType == BTargetDepType::FULL)
                                        {
                                            dependency->selectiveBuild = true;
                                        }
                                    }
                                }
                                else
                                {
                                    localReal.fileStatus = FileStatus::UPDATED;
                                }
                            }
                        }
                        else
                        {
                            // In rounds 2 and 1 all the targets will be updated.
                            // It is also considered that all targets have there fileStatus = FileStatus::NEEDS_UPDATE
                            // Index is only needed in round zero. Perform only for round one.
                            for (unsigned i = 0; i < TBT::topologicalSort.size(); ++i)
                            {
                                BTarget *bTarget_ = TBT::topologicalSort[i];
                                RealBTarget &r = bTarget_->getRealBTarget(round);
                                if (!r.dependenciesSize)
                                {
                                    updateBTargets.emplace_back(TBT::topologicalSort[i]);
                                }
                            }

                            updateBTargetsSizeGoal = TBT::topologicalSort.size();
                        }
                        updateBTargetsIterator = updateBTargets.begin();
                        builderMode = BuilderMode::UPDATE_BTARGET;
                        // printMessage(fmt::format("UnLocking Update Mutex {}\n", __LINE__));
                        updateMutex.unlock();
                        cond.notify_all();
                        // printMessage(fmt::format("Locking Update Mutex {}\n", __LINE__));
                        updateMutex.lock();
                    }
                }
                else
                {
                    // printMessage(fmt::format("{} {} {}\n", round, "presort-executing", getThreadId()));
                    bTarget = *preSortBTargetsIterator;
                    ++preSortBTargetsIterator;
                    // printMessage(fmt::format("UnLocking Update Mutex {}\n", __LINE__));
                    updateMutex.unlock();
                    cond.notify_all();
                    shouldBreak = true;
                }
            }
            break;
            case BuilderMode::UPDATE_BTARGET: {
                if (updateBTargetsIterator != updateBTargets.end())
                {
                    // printMessage(fmt::format("{} {} {}\n", round, "update-executing", getThreadId()));
                    bTarget = *updateBTargetsIterator;
                    realBTarget = &(bTarget->getRealBTarget(round));
                    ++updateBTargetsIterator;
                    // printMessage(fmt::format("UnLocking Update Mutex {}\n", __LINE__));
                    updateMutex.unlock();
                    cond.notify_all();
                    shouldBreak = true;
                }
                else if (updateBTargets.size() == updateBTargetsSizeGoal)
                {
                    if (!round && threadCount == numberOfLaunchedThreads)
                    {
                        // printMessage(fmt::format("UnLocking Update Mutex {}\n", __LINE__));
                        return;
                    }

                    ++threadCount;
                    // printMessage(fmt::format("{} {} {} {} {}\n", round, "update-exiting", threadCount,
                    //  numberOfLaunchedThreads, getThreadId()));
                    if (threadCount == numberOfLaunchedThreads)
                    {
                        if (round)
                        {
                            threadCount = 0;
                            nextMode = true;
                        }

                        if (round)
                        {
                            preSortBTargetsIterator = preSortBTargets.begin();
                            builderMode = BuilderMode::PRE_SORT;
                            --round;
                            cond.notify_all();
                        }
                        else
                        {
                            // printMessage("Exiting\n");
                            // printMessage(fmt::format("UnLocking Update Mutex {}\n", __LINE__));
                            updateMutex.unlock();
                            cond.notify_all();
                            updateMutex.lock();
                            // printMessage(fmt::format("UnLocking Update Mutex {}\n", __LINE__));
                            return;
                        }
                    }
                }
            }
            break;
            }

            if (shouldBreak)
            {
                break;
            }

#ifndef NDEBUG
            if (!nextMode)
            {
                // printMessage(fmt::format("{} {} {}\n", round, "Locking", getThreadId()));
                ++count;
                if (count == numberOfLaunchedThreads)
                {
                    if (updateBTargetsIterator != updateBTargets.end())
                    {
                        BTarget *b = *updateBTargetsIterator;
                        BTarget *c = b;
                    }
                    else
                    {
                        bool doubt = true;
                        for (BTarget *t : TBT::topologicalSort)
                        {
                            if (t->getRealBTarget(0).dependenciesSize)
                            {
                                bool bugFound = true;
                            }
                        }
                    }
                    bool breakpoint = true;
                }
                cond.wait(lk);
                --count;
            }
#else

            if (!nextMode)
            {
                // printMessage(fmt::format("{} {} {}\n", round, "Locking", getThreadId()));
                cond.wait(lk);
            }
#endif
        }

        switch (builderMode)
        {
        case BuilderMode::PRE_SORT:
            bTarget->preSort(*this, round);
            // printMessage(fmt::format("Locking Update Mutex {}\n", __LINE__));
            updateMutex.lock();
            break;

        case BuilderMode::UPDATE_BTARGET: {
            try
            {
                bTarget->updateBTarget(*this, round);
                // printMessage(fmt::format("Locking Update Mutex {}\n", __LINE__));
                updateMutex.lock();
                realBTarget->updateCalled = true;
                if (realBTarget->exitStatus != EXIT_SUCCESS)
                {
                    updateBTargetFailed = true;
                }
            }
            catch (std::exception &ec)
            {
                // printMessage(fmt::format("Locking Update Mutex {}\n", __LINE__));
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
            if (round)
            {
                for (auto &[dependent, bTargetDepType] : bTarget->getRealBTarget(round).dependents)
                {
                    RealBTarget &dependentRealBTarget = dependent->getRealBTarget(round);

                    if (realBTarget->exitStatus != EXIT_SUCCESS)
                    {
                        dependentRealBTarget.exitStatus = EXIT_FAILURE;
                    }
                    --(dependentRealBTarget.dependenciesSize);
                    if (!dependentRealBTarget.dependenciesSize)
                    {
                        updateBTargets.emplace_back(dependent);
                        if (updateBTargetsIterator == updateBTargets.end())
                        {
                            --updateBTargetsIterator;
                        }
                    }
                }
            }
            else
            {
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
                        if (!dependentRealBTarget.dependenciesSize && dependent->selectiveBuild)
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
