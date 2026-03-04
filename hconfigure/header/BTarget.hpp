/// \file
/// Define BTarget and RealBTarget, core classes of HMake

#ifndef HMAKE_BASICTARGETS_HPP
#define HMAKE_BASICTARGETS_HPP

#include "RunCommand.hpp"
#include "parallel-hashmap/parallel_hashmap/phmap.h"
#include <array>
#include <span>
#include <string>
#include <vector>

using std::size_t, std::vector, phmap::flat_hash_map, std::lock_guard, std::array, std::string, std::string_view;

class BTarget;

// TODO
// Maybe remove this and use vector with in-place sorting.
struct IndexInTopologicalSortComparatorRoundZero
{
    bool operator()(const BTarget *lhs, const BTarget *rhs) const;
};

struct IndexInTopologicalSortComparatorRoundTwo
{
    bool operator()(const BTarget *lhs, const BTarget *rhs) const;
};

/// Used in RealBTarget to specify the type of dependency of other RealBTargets.
enum class BTargetDepType : uint8_t
{
    /// Build-system will wait for the dependency to finish before completing the dependent. Plus it will set the
    /// dependent selectiveBuild if the dependency selectiveBuild is true
    FULL = 0,

    /// Build-system will wait but not set the selectiveBuild of dependent based on dependency.
    /// Unused currently
    WAIT = 1,

    /// Build-system will not wait but the selectiveBuild will be set. Used in specifying CppTarget dep with the other
    /// CppTarget as we want the dependency CppTarget's huDeps and imodDeps to be built.
    SELECTIVE = 2,

    /// Only for sorting. Used to specify static-lib dependency with other static-lib.
    LOOSE = 3,
};

/// Not related to the build-algorithm. This is used by different BTarget to synchronize printing with build-cache
/// updating.
enum class UpdateStatus : char
{
    ALREADY_UPDATED = 0,
    NEEDS_UPDATE = 1,
    UPDATED_WITHOUT_BUILDING = 2,
    UPDATED = 3,

};

/// Every BTarget has 2 of these so distinct dependency order can be specified for the 2 rounds.
class RealBTarget
{
    /// Contains RealBTarget* that form the cycle if there is any.
    inline static vector<RealBTarget *> cycle;

    /// it is set by sortGraph function if there is a cycle in the graph
    inline static bool cycleExists = false;

  public:
    /// This is not used by build-algorithm. This is used by CppMod to check whether a header-unit is built or not. This
    /// is also used by TargetCache::writeBuildCache function to determine whether cache could be updated or not. LOAT,
    /// CppSrc and CppMod in BTarget::completeRoundOne, assign this after exitStatus to ensure happens-before
    /// relationship in the signal-handler / console-handler function. Also, this is assigned before printing to avoid a
    /// situation where a target has printed the update but its cache is not yet updated in-case of build interruption.
    alignas(64) UpdateStatus updateStatus = UpdateStatus::ALREADY_UPDATED;

  private:
    // Used in BTarget::sortGraph
    uint32_t dependentsCount = 0;

    /// if there is a cycle, find out the exact RealBTarget involved
    static bool findCycleDFS(RealBTarget *node, phmap::flat_hash_set<RealBTarget *> &visited,
                             phmap::flat_hash_set<RealBTarget *> &recursionStack, vector<RealBTarget *> &currentPath,
                             string &errorString);

  public:
    /// sorted RealBTargets
    inline static vector<RealBTarget *> sorted;

    /// Input to the sortGraph function.
    inline static std::span<RealBTarget *> graphEdges;

    static void sortGraph();

    // for debugging
    static void printSortedGraph();

    flat_hash_map<RealBTarget *, BTargetDepType> dependents;
    flat_hash_map<RealBTarget *, BTargetDepType> dependencies;

    /// reverse pointer to the BTarget
    BTarget *bTarget = nullptr;

    /// Once sorted the index of this RealBTarget in the topological sorted array. Used in sorting and providing static
    /// libs in order as some linkers have this requirement.
    uint32_t indexInTopologicalSort = 0;

    /// This is incremented whenever a full-dependency or wait-dependency is added.
    /// It is decremented of the full-dependents or wait-dependents when the BTarget::completeRoundOne is completed
    /// And if it is zero for any of those dependents, those are added in Builder::updateBTargets list.
    uint32_t dependenciesSize = 0;

    // TODO
    //  Following describes the time taken for the completion of this task. Currently unused.
    // unsigned long timeTaken = 0;

    /// This is set to EXIT_FAILURE by the BTarget::completeRoundOne if the work fails. Builder::decrementFromDependents
    /// then assigns this value to the dependents as well.
    int exitStatus = EXIT_SUCCESS;

    // TODO
    // Some tools like modern linker or some common release management tasks like compression supports parallel
    // execution. But HMake can't use these tools without risking overflow of thread allocation compared to hardware
    // supported threads. Algorithm is to be developed which will call updateBTarget with the number of threads
    // alloted. It will work such that total allotment never crosses the number of hardware threads. Following two
    // variables will be used as guide for the algorithm

    // Some tasks have more potential to benefit from multiple threading than others. Or they maybe executing for longer
    // than others. Users will assign such higher priority and will be allocated more of  the thread-pool compared to
    // the other.

    // float potential = 0.5f;

    // How many threads the tool supports. -1 means any. some tools may not support more than a fixed number, so
    // updateBTarget will not be passed more than supportsThread

    // short supportsThread = -1;

    /// \param bTarget_ the back-pointer to BTarget that owns this
    /// \param round_ Constructor will add to BTarget::realBTargetsGlobal with the round index.
    RealBTarget(BTarget *bTarget_, unsigned short round_);

    /// \param bTarget_ the back-pointer to BTarget that owns this
    /// \param round_ Constructor will add to BTarget::realBTargetsGlobal[round]
    /// \param add whether to add for a round. Should be false if BTarget::completeRoundOne or
    /// BTarget::isEventRegistered is not going to do any work in that round.
    RealBTarget(BTarget *bTarget_, unsigned short round_, bool add);

    /// Assigns full-dependents RealBTarget::updateStatus with UpdateStatus::NEEDS_UPDATE.
    /// This is used by CppSrc and CppMod so that the LOAT does not have to make an extra check.
    void assignNeedsUpdateToDependents();
};

enum class BTargetType : unsigned short
{
    DEFAULT = 0,
    CPPMOD = 1,
    LINK_OR_ARCHIVE_TARGET = 2,
    CPP_TARGET = 3,
};

/// The building-block of HMake build-system
///
/// Any class that wants to perform work in order needs to inherit from this and override one of
/// BTarget::completeRoundOne, BTarget::isEventRegistered, BTarget::isEventCompleted functions. Dependencies between 2
/// BTargets can be specified using BTarget::addDep. Now the build-system will complete(call BTarget::completeRoundOne
/// in round 1 and isEventStarted in round 0) both BTarget in-order.
class BTarget // BTarget
{
    friend RealBTarget;

  public:
    /// All the RealBTargets of a round except those whose add is false. BTarget::completeRoundOne will be called for
    /// these in-order after sorting. This is populated by the RealBTarget constructor.
    inline static array<std::span<RealBTarget *>, 2> realBTargetsGlobal;

    // Maybe 3 of the following could be pointers
    /// count of BTarget::realBTargetsGlobal and the index where the pointer to the self will be added by the
    /// RealBTarget constructor.
    inline static array<uint32_t, 2> realBTargetsArrayCount{};

    /// run.startAsyncProcess should be called if the BTarget is to manage a child process in isEventRegistered
    /// function. run.output is the output of the child-process.
    RunCommand run;

  private:
    friend class Builder;
    friend void constructGlobals();

  public:
    alignas(64) inline static uint32_t total = 0;

    /// One RealBTarget for every round.
    array<RealBTarget, 2> realBTargets;

    string name;
    size_t id = 0; // unique for every BTarget

    // TODO
    // Following describes total time taken across all rounds. i.e. sum of all RealBTarget::timeTaken.
    // float totalTimeTaken = 0.0f;

    /// selectiveBuild is set for target if hbuild is executed in the target directory or the target name is provided to
    /// the build.exe or hbuild. It is set in BTarget::setSelectiveBuild which is called before
    /// BTarget::completeRoundOne of round1.
    bool selectiveBuild = false;

    /// if true selectiveBuild is only set if the target name is specified on cmd arguments.
    bool buildExplicit = false;

    BTarget();
    /// \param makeDirectory if true HMake will make the directory of the \p name_ in configureDir.
    BTarget(string name_, bool buildExplicit_, bool makeDirectory);
    /// \p add0 and \p add1 specify the value for RealBTarget constructors for the 2 rounds.
    BTarget(bool add0, bool add1);
    /// \param makeDirectory if true HMake will make the directory of the \p name_ in configureDir.
    BTarget(string name_, bool buildExplicit_, bool makeDirectory, bool add0, bool add1);

    virtual ~BTarget();

    /// Might set the BTarget::selectiveBuild variable based on BTarget::name, hbuild execution directory and passed
    /// arguments.
    void setSelectiveBuild();

    /// returns true if hbuild is executed in same or child directory based on BTarget::name.
    bool isHBuildInSameOrChildDirectory() const;

    /// Used by findCycleDFS to report RealBTarget in cycle
    virtual string getPrintName() const;

    /// Can be overridden to differentiate between different BTarget class objects
    virtual BTargetType getBTargetType() const;

    /// This is called by Builder in-order. Should be overridden to perform any work.
    virtual void completeRoundOne();

    /// This is called by Builder in order in BSMode::BUILD in round 0. This should be overridden to perform work in
    /// round 0 by launching a new process.
    /// \return true if the BTarget has launched a new process using run.startyAsyncProcess. If this function returns
    /// false, Builder::decrementFromDependents is called. Otherwise, it is assumed that the BTarget is waiting on the
    /// process event.
    virtual bool isEventRegistered(Builder &builder);

    /// This function will be called only if the child process exited, or it is waiting on input after printing on
    /// stdout, followed by the delimiter. In case, the process exited, the following is called after reaping the
    /// process and realBTargets[0].exitStatus is also set to the exitStatus of the process.
    /// \message Child-process message to the build-system. if this is empty, it means that the child-process has
    /// exited, and it has been reaped. In that case, run.output is the output of the child process.
    /// \return true if the BTarget has registered an event in the event loop of Builder. This can be done using Builder
    /// function Builder::registerEventData. If this function returns false, Builder::decrementFromDependents is
    /// called. Otherwise, it is assumed that the BTarget is waiting on an event.
    virtual bool isEventCompleted(Builder &builder, string_view message);

    /// Does both steps. Should be called only in single-thread or under Builder::executeMutex in
    /// BTarget::completeRoundOne
    template <unsigned short round, BTargetDepType depType = BTargetDepType::FULL> void addDep(BTarget &dep);
};
bool operator<(const BTarget &lhs, const BTarget &rhs);

template <unsigned short round, BTargetDepType depType> void BTarget::addDep(BTarget &dep)
{
    if (realBTargets[round].dependencies.try_emplace(&dep.realBTargets[round], depType).second)
    {
        dep.realBTargets[round].dependents.try_emplace(&this->realBTargets[round], depType);
        if constexpr (depType == BTargetDepType::FULL || depType == BTargetDepType::WAIT)
        {
            ++realBTargets[round].dependenciesSize;
        }
    }
}

#endif // HMAKE_BASICTARGETS_HPP
