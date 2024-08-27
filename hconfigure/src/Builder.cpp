
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
        if (!atomic_ref(target->realBTargets[round].dependenciesSize).load())
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

    // printMessage(fmt::format("{} Locking Update Mutex {} {}\n", round, __LINE__, getThreadId()));
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
                counted = false;
                // printMessage(fmt::format("{} update-executing {} {}\n", round, __LINE__, getThreadId()));
                bTarget = *updateBTargetsIterator;
                realBTarget = &bTarget->realBTargets[round];
                ++updateBTargetsIterator;
                // printMessage(fmt::format("{} UnLocking Update Mutex {} {}\n", round, __LINE__, getThreadId()));
                executeMutex.unlock();
                cond.notify_one();
                shouldBreak = true;
            }
            else if (updateBTargets.size() == updateBTargetsSizeGoal && !counted)
            {
                counted = true;

                // printMessage(fmt::format("{} updateBTargets.size() == updateBTargetsSizeGoal {} {} {}\n", round,
                //  numberOfSleepingThreadsCounted.load() + numberOfSleepingThreads.load(),
                // numberOfLaunchedThreads, getThreadId()));
                if (numberOfSleepingThreadsCounted + numberOfSleepingThreads == numberOfLaunchedThreads - 1)
                {
                    /*// printMessage(fmt::format("{} {} {}\n", round, "UPDATE_BTARGET threadCount ==
                       numberOfLaunchThreads", getThreadId()));*/

                    if (round > roundGoal && !errorHappenedInRoundMode)
                    {
                        --round;
                        TBT::tarjanNodes = &tarjanNodesBTargets[round];
                        TBT::findSCCS();
                        TBT::checkForCycle();

                        updateBTargets.clear();
                        updateBTargetsSizeGoal = 0;

                        if (!round)
                        {
                            if (const size_t topSize = TBT::topologicalSort.size())
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

                                    if (!atomic_ref(localReal.dependenciesSize).load())
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
                            for (uint64_t i = 0; i < TBT::topologicalSort.size(); ++i)
                            {
                                if (!atomic_ref(TBT::topologicalSort[i]->realBTargets[round].dependenciesSize).load())
                                {
                                    updateBTargets.emplace_back(TBT::topologicalSort[i]);
                                }
                                TBT::topologicalSort[i]->realBTargets[round].indexInTopologicalSort = i;
                            }
                            /*for (BTarget *target : TBT::topologicalSort)
                            {
                                if (!target->realBTargets[round].dependenciesSize)
                                {
                                    updateBTargets.emplace_back(target);
                                }
                            }*/
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
                    cond.notify_one();
                    // printMessage(fmt::format("{} Locking after notifying one after round decrement {} {}\n", round,
                    // __LINE__, getThreadId()));
                    executeMutex.lock();
                    if (returnAfterWakeup)
                    {
                        // printMessage(fmt::format("{} Returning after roundGoal Achieved{} {}\n", round, __LINE__,
                        // getThreadId()));
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
                incrementNumberOfSleepingThreads(counted);
                // printMessage(fmt::format("{} Condition waiting {} {}\n", round, __LINE__, getThreadId()));
                cond.wait(lk);
                decrementNumberOfSleepingThreads(counted);
                // printMessage(fmt::format("{} Wakeup after condition waiting {} {}\n", round, __LINE__,
                // getThreadId()));
                if (roundLocal != round)
                {
                    counted = false;
                }
                if (returnAfterWakeup)
                {
                    cond.notify_one();
                    // printMessage(fmt::format("{} returning after wakeup from condition variable {} {}\n", round,
                    // __LINE__, getThreadId()));
                    return;
                }
            }
        }

        try
        {
            bTarget->updateBTarget(*this, round);
            // printMessage(fmt::format("{} Locking in try block {} {}\n", round, __LINE__, getThreadId()));
            executeMutex.lock();
            if (realBTarget->exitStatus != EXIT_SUCCESS)
            {
                errorHappenedInRoundMode = true;
            }
        }
        catch (std::exception &ec)
        {
            // printMessage(fmt::format("{} Locking in catch block {} {}\n", round, __LINE__, getThreadId()));
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
    }
}

void Builder::incrementNumberOfSleepingThreads(const bool counted)
{
    if (!counted)
    {
        ++numberOfSleepingThreads;
    }
    else
    {
        ++numberOfSleepingThreadsCounted;
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

void Builder::decrementNumberOfSleepingThreads(bool counted)
{
    if (!counted)
    {
        --numberOfSleepingThreads;
    }
    else
    {
        --numberOfSleepingThreadsCounted;
    }
}