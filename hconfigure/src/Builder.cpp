
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
    RealBTarget::tarjanNodes = span(BTarget::tarjanNodesBTargets[round].data(), BTarget::tarjanNodesCount[round]);
    RealBTarget::findSCCS(round);
    RealBTarget::checkForCycle();

    for (BTarget *target : RealBTarget::topologicalSort)
    {
        if (!target->realBTargets[round].dependenciesSize)
        {
            updateBTargets.emplaceBackBeforeRound(target);
        }
    }

    updateBTargets.initializeForRound(round);
    updateBTargetsSizeGoal = RealBTarget::topologicalSort.size();

    vector<thread *> threads;

    if (const unsigned int launchThreads = settings.maximumBuildThreads; launchThreads)
    {
        numberOfLaunchedThreads = launchThreads;

        for (uint64_t i = 0; i < launchThreads - 1; ++i)
        {
            BTarget::centralRegistryForTwoBTargetsVector.emplace_back(nullptr);
        }

        while (threads.size() != launchThreads - 1)
        {
            uint64_t index = threads.size() + 1;
            threads.emplace_back(new thread([this, index] {
                BTarget::centralRegistryForTwoBTargetsVector[index] = &BTarget::twoBTargetsVector;
                execute();
            }));
        }
        execute();
    }
    else
    {
        printErrorMessage("maximumBuildThreads is zero\n");
        errorExit();
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

            if (bTarget = updateBTargets.getItem(); bTarget)
            {
                DEBUG_EXECUTE(FORMAT("{} update-executing {} {}\n", round, __LINE__, getThreadId()));
                realBTarget = &bTarget->realBTargets[round];
                DEBUG_EXECUTE(FORMAT("{} UnLocking Update Mutex {} {}\n", round, __LINE__, getThreadId()));
                executeMutex.unlock();
                cond.notify_one();
                shouldBreak = true;
            }
            else if (updateBTargets.size() == updateBTargetsSizeGoal)
            {
                DEBUG_EXECUTE(FORMAT("{} updateBTargets.size() == updateBTargetsSizeGoal {} {} {}\n", round,
                                     numberOfSleepingThreads.load(), numberOfLaunchedThreads, getThreadId()));
                if (numberOfSleepingThreads == numberOfLaunchedThreads - 1)
                {
                    singleThreadRunning = true;
                    DEBUG_EXECUTE(FORMAT("{} {} {}\n", round, "UPDATE_BTARGET threadCount == numberOfLaunchThreads",
                                         getThreadId()));

                    BTarget::runEndOfRoundTargets(*this, round);
                    if (round > roundGoal && !errorHappenedInRoundMode)
                    {
                        --round;
                        RealBTarget::tarjanNodes =
                            span(BTarget::tarjanNodesBTargets[round].data(), BTarget::tarjanNodesCount[round]);
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

                                    if (!localReal.dependenciesSize)
                                    {
                                        updateBTargets.emplaceFrontBeforeLastRound(&localBTarget);
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
                                if (!RealBTarget::topologicalSort[i]->realBTargets[round].dependenciesSize)
                                {
                                    updateBTargets.emplaceBackBeforeRound(RealBTarget::topologicalSort[i]);
                                }
                                RealBTarget::topologicalSort[i]->realBTargets[round].indexInTopologicalSort = i;
                            }
                        }

                        updateBTargetsSizeGoal = RealBTarget::topologicalSort.size();
                        updateBTargets.initializeForRound(round);
                    }
                    else
                    {
                        returnAfterWakeup = true;
                    }

                    singleThreadRunning = false;
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
        bool isComplete = false;
        bTarget->updateBTarget(*this, round, isComplete);
        if (isComplete)
        {
            continue;
        }
        executeMutex.lock();
        addNewTopBeUpdatedTargets(bTarget);
    }
}

void Builder::addNewTopBeUpdatedTargets(BTarget *bTarget)
{
    const RealBTarget *realBTarget = &bTarget->realBTargets[round];
    DEBUG_EXECUTE(FORMAT("{} Locking in try block {} {}\n", round, __LINE__, getThreadId()));
    if (realBTarget->exitStatus != EXIT_SUCCESS)
    {
        errorHappenedInRoundMode = true;
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
            --dependentRealBTarget.dependenciesSize;
            if (!dependentRealBTarget.dependenciesSize)
            {
                updateBTargets.emplace(dependent);
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
                    updateBTargets.emplace(dependent);
                }
            }
        }
    }

    DEBUG_EXECUTE(FORMAT("{} {} Info: updateBTargets.size() {} updateBTargetsSizeGoal {} {}\n", round, __LINE__,
                         updateBTargets.size(), updateBTargetsSizeGoal, getThreadId()));
}

void Builder::incrementNumberOfSleepingThreads()
{
    if (numberOfSleepingThreads == numberOfLaunchedThreads - 1)
    {
        RealBTarget::clearTarjanNodes();
        RealBTarget::findSCCS(round);

        // If a cycle happened this will throw an error, otherwise following exception will be thrown.
        RealBTarget::checkForCycle();

        printErrorMessage("HMake API misuse.\n");
    }
    ++numberOfSleepingThreads;
}

void Builder::decrementNumberOfSleepingThreads()
{
    --numberOfSleepingThreads;
}