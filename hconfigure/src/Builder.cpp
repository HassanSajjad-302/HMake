
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

vector<std::thread::id> threadIds;

void Builder::execute()
{
    BTarget *bTarget = nullptr;
    RealBTarget *realBTarget = nullptr;

    // printMessage(fmt::format("Locking Update Mutex {}\n", __LINE__));
    std::unique_lock<std::mutex> lk(updateMutex);
    BuilderMode builderModeLocal = builderMode;
    unsigned short roundLocal = round;
    while (true)
    {
        bool counted = false;
        while (true)
        {
            bool shouldBreak = false;
            bool nextMode = false;

            switch (builderMode)
            {
            case BuilderMode::PRE_SORT: {
                if (preSortBTargetsIterator != preSortBTargets.end())
                {
                    // printMessage(fmt::format("{} {} {}\n", round, "presort-executing", getThreadId()));
                    bTarget = *preSortBTargetsIterator;
                    ++preSortBTargetsIterator;
                    // printMessage(fmt::format("UnLocking Update Mutex {}\n", __LINE__));
                    updateMutex.unlock();
                    cond.notify_all();
                    shouldBreak = true;
                }
                else if (!counted && preSortBTargetsIterator == preSortBTargets.end())
                {
                    ++threadCount;
                    counted = true;
                    if (!round)
                    {
                        threadIds.emplace_back(std::this_thread::get_id());
                    }

                    /*                    printMessage(fmt::format("{} {} {} {} {}\n", round,
                                                                 "preSotBTargetIterator == preSortBTargets.end()",
                       threadCount, numberOfLaunchedThreads, getThreadId()));*/
                    if (threadCount == numberOfLaunchedThreads)
                    {
                        /*                        printMessage(fmt::format("{} {} {}\n", round, "PRE_SORT threadCount ==
                           numberOfLaunchThreads", getThreadId()));*/
                        threadCount = 0;
                        nextMode = true;

                        auto &k = tarjanNodesBTargets.try_emplace(round, set<TBT>()).first->second;
                        TBT::tarjanNodes = &(k);
                        TBT::findSCCS();
                        TBT::checkForCycle();

                        updateBTargets.clear();
                        updateBTargetsSizeGoal = 0;

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
                            }
                        }
                        else
                        {
                            // In rounds 2 and 1 all the targets will be updated.
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
                        builderModeLocal = builderMode;
                        counted = false;
                        // printMessage(fmt::format("UnLocking Update Mutex {}\n", __LINE__));
                        updateMutex.unlock();
                        cond.notify_all();
                        // printMessage(fmt::format("Locking Update Mutex {}\n", __LINE__));
                        updateMutex.lock();
                    }
                }
            }
            break;
            case BuilderMode::UPDATE_BTARGET: {
                if (updateBTargetsIterator != updateBTargets.end())
                {
                    // This can be true when a thread has already added in threadCount but then later a new btarget was
                    // appended to the updateBTargets by function like addNewBTargetInFinalBTargets
                    if (counted)
                    {
                        --threadCount;
                        counted = false;
                    }
                    // printMessage(fmt::format("{} {} {}\n", round, "update-executing", getThreadId()));
                    bTarget = *updateBTargetsIterator;
                    realBTarget = &(bTarget->getRealBTarget(round));
                    ++updateBTargetsIterator;
                    // printMessage(fmt::format("UnLocking Update Mutex {}\n", __LINE__));
                    updateMutex.unlock();
                    cond.notify_all();
                    shouldBreak = true;
                }
                else if (updateBTargets.size() == updateBTargetsSizeGoal && !counted)
                {
                    if (!round && threadCount == numberOfLaunchedThreads)
                    {
                        // printMessage(fmt::format("UnLocking Update Mutex {}\n", __LINE__));
                        return;
                    }

                    ++threadCount;
                    if (round)
                    {
                        counted = true;
                    }
                    /*                     printMessage(fmt::format("{} {} {} {} {}\n", round,
                                                                 "updateBTargets.size() == updateBTargetsSizeGoal",
                       threadCount, numberOfLaunchedThreads, getThreadId()));*/
                    if (threadCount == numberOfLaunchedThreads)
                    {
                        /*                        printMessage(fmt::format("{} {} {}\n", round,
                                                                         "UPDATE_BTARGET threadCount ==
                           numberOfLaunchThreads", getThreadId()));*/

                        if (round)
                        {
                            nextMode = true;
                            threadCount = 0;
                            preSortBTargetsIterator = preSortBTargets.begin();
                            builderMode = BuilderMode::PRE_SORT;
                            builderModeLocal = builderMode;
                            counted = false;
                            --round;
                            roundLocal = round;
                            updateMutex.unlock();
                            cond.notify_all();
                            updateMutex.lock();
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

            if (!nextMode)
            {
                // printMessage(fmt::format("{} {} {}\n", round, "Locking", getThreadId()));
                cond.wait(lk);
                if (builderModeLocal != builderMode || roundLocal != round)
                {
                    builderModeLocal = builderMode;
                    roundLocal = round;
                    counted = false;
                }
            }
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
            if (target->targetCacheChanged.load(std::memory_order_acquire))
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
