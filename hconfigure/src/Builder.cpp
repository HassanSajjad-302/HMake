
#ifdef USE_HEADER_UNITS
import "Builder.hpp";
import "BTarget.hpp";
import "Settings.hpp";
import "Utilities.hpp";
import <mutex>;
import <stack>;
import <thread>;
#else
#include "Builder.hpp"
#include "BTarget.hpp"
#include "Settings.hpp"
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
    RealBTarget::tarjanNodes = &BTarget::tarjanNodesBTargets[round];
    RealBTarget::findSCCS(round);
    RealBTarget::checkForCycle();

    for (BTarget *target : RealBTarget::topologicalSort)
    {
        if (!atomic_ref(target->realBTargets[round].dependenciesSize).load())
        {
            updateBTargets.emplace_back(target);
        }
    }

    updateBTargetsIterator = updateBTargets.begin();
    updateBTargetsSizeGoal = RealBTarget::topologicalSort.size();

    vector<thread *> threads;

    if (const unsigned int launchThreads = settings.maximumBuildThreads; launchThreads)
    {
        numberOfLaunchedThreads = launchThreads;
        while (threads.size() != launchThreads - 1)
        {
            threads.emplace_back(new thread{&Builder::execute, this});
        }
        execute();
    }
    else
    {
        printErrorMessage("maximumBuildThreads is zero\n");
        exit(EXIT_FAILURE);
    }
    for (thread *t : threads)
    {
        t->join();
        delete t;
    }
}

// #define DEBUG_EXECUTE_YES

#ifdef DEBUG_EXECUTE_YES
#define DEBUG_EXECUTE(x) printMessage(x)
#else
#define DEBUG_EXECUTE(x)
#endif

extern string getThreadId();

#ifndef NDEBUG
unsigned short count = 0;
#endif

vector<std::thread::id> threadIds;

void Builder::execute()
{
    BTarget *bTarget = nullptr;
    const RealBTarget *realBTarget = nullptr;

    DEBUG_EXECUTE(
        FORMAT("{} Locking Update Mutex {} {} {}\n", round, __LINE__, numberOfSleepingThreads.load(), getThreadId()));
    std::unique_lock lk(executeMutex);
    while (true)
    {
        while (true)
        {
            const unsigned short roundLocal = round;
            bool shouldBreak = false;

            if (updateBTargetsIterator != updateBTargets.end())
            {
                DEBUG_EXECUTE(FORMAT("{} update-executing {} {}\n", round, __LINE__, getThreadId()));
                bTarget = *updateBTargetsIterator;
                realBTarget = &bTarget->realBTargets[round];
                ++updateBTargetsIterator;
                DEBUG_EXECUTE(FORMAT("{} UnLocking Update Mutex {} {}\n", round, __LINE__, getThreadId()));
                executeMutex.unlock();
                cond.notify_one();
                shouldBreak = true;
            }
            else if (updateBTargets.size() == updateBTargetsSizeGoal)
            {
                DEBUG_EXECUTE(FORMAT("{} updateBTargets.size() == updateBTargetsSizeGoal {} {} {}\n", round,
                                     numberOfSleepingThreads.load(), numberOfLaunchedThreads, getThreadId()));
                if (numberOfSleepingThreads.load() == numberOfLaunchedThreads - 1)
                {
                    DEBUG_EXECUTE(FORMAT("{} {} {}\n", round, "UPDATE_BTARGET threadCount == numberOfLaunchThreads",
                                         getThreadId()));

                    BTarget::runEndOfRoundTargets(*this, round);
                    if (round > roundGoal && !errorHappenedInRoundMode)
                    {
                        --round;
                        RealBTarget::tarjanNodes = &BTarget::tarjanNodesBTargets[round];
                        RealBTarget::findSCCS(round);
                        RealBTarget::checkForCycle();

                        updateBTargets.clear();
                        updateBTargetsSizeGoal = 0;

                        if (!round)
                        {
                            if (const size_t topSize = RealBTarget::topologicalSort.size())
                            {
                                for (size_t i = RealBTarget::topologicalSort.size(); i-- > 0;)
                                {
                                    BTarget &localBTarget = *RealBTarget::topologicalSort[i];
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

                                    if (!atomic_ref(localReal.dependenciesSize).load())
                                    {
                                        updateBTargets.emplace_front(&localBTarget);
                                    }
                                }
                            }

                            updateBTargetsSizeGoal = RealBTarget::topologicalSort.size();
                        }
                        else
                        {
                            // In rounds 2 and 1 all the targets will be updated.
                            // Index is only needed in round zero. Perform only for round one.
                            for (uint64_t i = 0; i < RealBTarget::topologicalSort.size(); ++i)
                            {
                                if (!atomic_ref(RealBTarget::topologicalSort[i]->realBTargets[round].dependenciesSize)
                                         .load())
                                {
                                    updateBTargets.emplace_back(RealBTarget::topologicalSort[i]);
                                }
                                RealBTarget::topologicalSort[i]->realBTargets[round].indexInTopologicalSort = i;
                            }
                        }

                        updateBTargetsSizeGoal = RealBTarget::topologicalSort.size();
                        updateBTargetsIterator = updateBTargets.begin();
                    }
                    else
                    {
                        returnAfterWakeup = true;
                    }

                    executeMutex.unlock();
                    cond.notify_one();
                    DEBUG_EXECUTE(FORMAT("{} Locking after notifying one after round decrement {} {}\n", round,
                                         __LINE__, getThreadId()));
                    executeMutex.lock();
                    if (returnAfterWakeup)
                    {
                        DEBUG_EXECUTE(
                            FORMAT("{} Returning after roundGoal Achieved{} {}\n", round, __LINE__, getThreadId()));
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
                DEBUG_EXECUTE(FORMAT("{} Condition waiting {} {} {}\n", round, __LINE__, numberOfSleepingThreads.load(),
                                     getThreadId()));
                incrementNumberOfSleepingThreads();
                cond.wait(lk);
                decrementNumberOfSleepingThreads();
                DEBUG_EXECUTE(FORMAT("{} Wakeup after condition waiting {} {} {} \n", round, __LINE__,
                                     numberOfSleepingThreads.load(), getThreadId()));
                if (returnAfterWakeup)
                {
                    cond.notify_one();
                    DEBUG_EXECUTE(FORMAT("{} returning after wakeup from condition variable {} {}\n", round, __LINE__,
                                         getThreadId()));
                    return;
                }
            }
        }

        if (round == 2)
        {
            bTarget->setSelectiveBuild();
        }
        bTarget->updateBTarget(*this, round);
        DEBUG_EXECUTE(FORMAT("{} Locking in try block {} {}\n", round, __LINE__, getThreadId()));
        executeMutex.lock();
        if (realBTarget->exitStatus != EXIT_SUCCESS)
        {
            errorHappenedInRoundMode = true;
        }

        // bTargetDepType is only considered in round 0.
        if (!realBTarget->isUpdated)
        {
            continue;
        }
        if (round)
        {
            for (auto &[dependent, bTargetDepType] : bTarget->realBTargets[round].dependents)
            {
                RealBTarget &dependentRealBTarget = dependent->realBTargets[round];

                if (realBTarget->exitStatus != EXIT_SUCCESS)
                {
                    dependentRealBTarget.exitStatus = EXIT_FAILURE;
                }
                --atomic_ref(dependentRealBTarget.dependenciesSize);
                if (!atomic_ref(dependentRealBTarget.dependenciesSize).load())
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
                    --atomic_ref(dependentRealBTarget.dependenciesSize);
                    if (!atomic_ref(dependentRealBTarget.dependenciesSize).load())
                    {
                        updateBTargetsIterator = updateBTargets.emplace(updateBTargetsIterator, dependent);
                    }
                }
            }
        }

        DEBUG_EXECUTE(FORMAT("{} {} Info: updateBTargets.size() {} updateBTargetsSizeGoal {} {}\n", round, __LINE__,
                             updateBTargets.size(), updateBTargetsSizeGoal, getThreadId()));
    }
}

void Builder::incrementNumberOfSleepingThreads()
{
    if (numberOfSleepingThreads.fetch_add(1) == numberOfLaunchedThreads - 1)
    {
        RealBTarget::clearTarjanNodes();
        RealBTarget::findSCCS(round);

        // If a cycle happened this will throw an error, otherwise following exception will be thrown.
        RealBTarget::checkForCycle();

        printErrorMessage("HMake API misuse.\n");
    }
}

void Builder::decrementNumberOfSleepingThreads()
{
    --numberOfSleepingThreads;
}