
#ifdef USE_HEADER_UNITS
import "Builder.hpp";
import "BTarget.hpp";
import "Utilities.hpp";
import <mutex>;
import <stack>;
import <thread>;
#else
#include "Builder.hpp"
#include "BTarget.hpp"
#include "Utilities.hpp"
#include <mutex>
#include <stack>
#include <thread>
#endif
using std::thread, std::mutex, std::make_unique, std::unique_ptr, std::ifstream, std::ofstream, std::stack,
    std::filesystem::current_path;

Builder::Builder()
{
    round = 2;
    roundGoal = bsMode == BSMode::BUILD ? 0 : 2;

    TBT::tarjanNodes = &tarjanNodesBTargets[round];
    TBT::findSCCS();
    TBT::checkForCycle();

    for (BTarget *target : TBT::topologicalSort)
    {
        if (!target->realBTargets[round].dependenciesSize)
        {
            updateBTargets.emplace_back(target);
        }
        target->setSelectiveBuild();
    }

    updateBTargetsIterator = updateBTargets.begin();
    updateBTargetsSizeGoal = TBT::topologicalSort.size();

    vector<thread *> threads;

    const unsigned int launchThreads = settings.maximumBuildThreads;
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

extern pstring getThreadId();

#ifndef NDEBUG
unsigned short count = 0;
#endif

vector<std::thread::id> threadIds;

void Builder::execute()
{
    BTarget *bTarget = nullptr;
    RealBTarget *realBTarget = nullptr;

    // printMessage(fmt::format("Locking Update Mutex {}\n", __LINE__));
    std::unique_lock lk(executeMutex);
    while (true)
    {
        bool counted = false;
        while (true)
        {
            const unsigned short roundLocal = round;
            bool shouldBreak = false;

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
                realBTarget = &bTarget->realBTargets[round];
                ++updateBTargetsIterator;
                // printMessage(fmt::format("UnLocking Update Mutex {}\n", __LINE__));
                executeMutex.unlock();
                cond.notify_all();
                shouldBreak = true;
            }
            else if (updateBTargets.size() == updateBTargetsSizeGoal && !counted)
            {
                ++threadCount;
                counted = true;

                /* printMessage(fmt::format("{} {} {} {} {}\n", round, "updateBTargets.size() ==
                   updateBTargetsSizeGoal", threadCount, numberOfLaunchedThreads, getThreadId()));*/
                if (threadCount == numberOfLaunchedThreads)
                {
                    /*printMessage(fmt::format("{} {} {}\n", round, "UPDATE_BTARGET threadCount ==
                       numberOfLaunchThreads", getThreadId()));*/

                    if (round > roundGoal && !errorHappenedInRoundMode)
                    {
                        --round;
                        threadCount = 0;
                        TBT::tarjanNodes = &tarjanNodesBTargets[round];
                        TBT::findSCCS();
                        TBT::checkForCycle();

                        updateBTargets.clear();
                        updateBTargetsSizeGoal = 0;

                        if (!round)
                        {
                            const size_t topSize = TBT::topologicalSort.size();
                            if (topSize)
                            {

                                for (size_t i = TBT::topologicalSort.size(); i-- > 0;)
                                {
                                    BTarget &localBTarget = *TBT::topologicalSort[i];
                                    RealBTarget &localReal = localBTarget.realBTargets[0];

                                    localReal.indexInTopologicalSort = topSize - (i + 1);
                                    if (localBTarget.selectiveBuild)
                                    {
                                        for (auto &[dependency, bTargetDepType] : localReal.dependencies)
                                        {
                                            if (bTargetDepType == BTargetDepType::FULL)
                                            {
                                                dependency->selectiveBuild = true;
                                            }
                                        }
                                    }

                                    if (!localReal.dependenciesSize)
                                    {
                                        updateBTargets.emplace_front(&localBTarget);
                                    }
                                }
                            }

                            updateBTargetsSizeGoal = TBT::topologicalSort.size();
                        }
                        else
                        {
                            // In rounds 2 and 1 all the targets will be updated.
                            // Index is only needed in round zero. Perform only for round one.
                            for (BTarget *target : TBT::topologicalSort)
                            {
                                if (!target->realBTargets[round].dependenciesSize)
                                {
                                    updateBTargets.emplace_back(target);
                                }
                            }
                        }

                        updateBTargetsSizeGoal = TBT::topologicalSort.size();
                        updateBTargetsIterator = updateBTargets.begin();
                        counted = false;
                    }
                    else
                    {
                        returnAfterWakeup = true;
                    }

                    executeMutex.unlock();
                    cond.notify_all();
                    executeMutex.lock();
                    if (returnAfterWakeup)
                    {
                        return;
                    }
                }
            }

            if (shouldBreak)
            {
                break;
            }

            if (roundLocal == round)
            {
                // printMessage(fmt::format("{} {} {}\n", round, "Locking", getThreadId()));
                incrementNumberOfSleepingThreads(counted);
                cond.wait(lk);
                if (!counted)
                {
                    --numberOfSleepingThreads;
                }
                if (roundLocal != round)
                {
                    counted = false;
                }
                if (returnAfterWakeup)
                {
                    return;
                }
            }
        }

        try
        {
            bTarget->updateBTarget(*this, round);
            // printMessage(fmt::format("Locking Update Mutex {}\n", __LINE__));
            executeMutex.lock();
            if (realBTarget->exitStatus != EXIT_SUCCESS)
            {
                errorHappenedInRoundMode = true;
            }
        }
        catch (std::exception &ec)
        {
            // printMessage(fmt::format("Locking Update Mutex {}\n", __LINE__));
            executeMutex.lock();
            realBTarget->exitStatus = EXIT_FAILURE;
            string str(ec.what());
            if (!str.empty())
            {
                printErrorMessage(str);
            }
            if (realBTarget->exitStatus != EXIT_SUCCESS)
            {
                errorHappenedInRoundMode = true;
            }
        }

        // bTargetDepType is only considered in round 0.
        if (round)
        {
            for (auto &[dependent, bTargetDepType] : bTarget->realBTargets[round].dependents)
            {
                RealBTarget &dependentRealBTarget = dependent->realBTargets[round];

                if (realBTarget->exitStatus != EXIT_SUCCESS)
                {
                    dependentRealBTarget.exitStatus = EXIT_FAILURE;
                }
                --dependentRealBTarget.dependenciesSize;
                if (!dependentRealBTarget.dependenciesSize)
                {
                    updateBTargetsIterator = updateBTargets.emplace(updateBTargetsIterator, dependent);
                }
            }
        }
        else
        {
            for (auto &[dependent, bTargetDepType] : bTarget->realBTargets[round].dependents)
            {
                RealBTarget &dependentRealBTarget = dependent->realBTargets[round];
                if (bTargetDepType == BTargetDepType::FULL)
                {
                    if (realBTarget->exitStatus != EXIT_SUCCESS)
                    {
                        dependentRealBTarget.exitStatus = EXIT_FAILURE;
                    }
                    --dependentRealBTarget.dependenciesSize;
                    if (!dependentRealBTarget.dependenciesSize)
                    {
                        updateBTargetsIterator = updateBTargets.emplace(updateBTargetsIterator, dependent);
                    }
                }
            }
        }
    }
}

void Builder::incrementNumberOfSleepingThreads(const bool counted)
{
    if (!counted)
    {
        ++numberOfSleepingThreads;
    }
    if (numberOfSleepingThreads == numberOfLaunchedThreads)
    {
        try
        {
            TBT::clearTarjanNodes();
            TBT::findSCCS();

            // If a cycle happened this will throw an error, otherwise following exception will be thrown.
            TBT::checkForCycle();

            throw std::runtime_error("HMake API misuse.\n");
        }
        catch (std::exception &ec)
        {
            // printMessage(fmt::format("Locking Update Mutex {}\n", __LINE__));
            const string str(ec.what());
            if (!str.empty())
            {
                printErrorMessage(str);
            }
            exit(EXIT_FAILURE);
        }
    }
}