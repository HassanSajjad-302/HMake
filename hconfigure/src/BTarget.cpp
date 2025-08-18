
#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
import "BuildSystemFunctions.hpp";
import "CppSourceTarget.hpp";
import "TargetCacheDiskWriteManager.hpp";
import <filesystem>;
import <utility>;
#else
#include "BTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include "TargetCacheDiskWriteManager.hpp"
#include <filesystem>
#include <utility>
#endif
using std::filesystem::create_directories, std::ofstream, std::filesystem::current_path, std::mutex, std::lock_guard,
    std::filesystem::create_directory;

BTarget::StaticInitializationTarjanNodesBTargets::StaticInitializationTarjanNodesBTargets()
{
    // 1MB. Deallocated after round.
    for (span<RealBTarget *> &realBTargets : tarjanNodesBTargets)
    {
        const auto buffer = new char[1024 * 1024 * 2]; // 2 MB
        realBTargets = span(reinterpret_cast<RealBTarget **>(buffer), 1024 * 1024 * 2 / sizeof(RealBTarget *));
    }
}

bool IndexInTopologicalSortComparatorRoundZero::operator()(const BTarget *lhs, const BTarget *rhs) const
{
    return const_cast<BTarget *>(lhs)->realBTargets[0].indexInTopologicalSort <
           const_cast<BTarget *>(rhs)->realBTargets[0].indexInTopologicalSort;
}

bool IndexInTopologicalSortComparatorRoundTwo::operator()(const BTarget *lhs, const BTarget *rhs) const
{
    return const_cast<BTarget *>(lhs)->realBTargets[2].indexInTopologicalSort <
           const_cast<BTarget *>(rhs)->realBTargets[2].indexInTopologicalSort;
}

void RealBTarget::clearTarjanNodes()
{
    for (RealBTarget *i : tarjanNodes)
    {
        if (i)
        {
            i->nodeIndex = 0;
            i->lowLink = 0;
            i->initialized = false;
            i->onStack = false;
        }
    }
}

void RealBTarget::findSCCS(unsigned short round)
{
    index = 0;
    cycleExists = false;
    cycle.clear();
    nodesStack.clear();
    topologicalSort.clear();
    for (RealBTarget *tarjanNode : tarjanNodes)
    {
        if (tarjanNode && !tarjanNode->initialized)
        {
            tarjanNode->strongConnect(round);
        }
    }
}

void RealBTarget::strongConnect(unsigned short round)
{
    initialized = true;
    nodeIndex = index;
    lowLink = index;
    ++index;
    nodesStack.emplace_back(this);
    onStack = true;

    for (auto &[bTarget, bTargetDepType] : dependencies)
    {
        RealBTarget &tarjandep = bTarget->realBTargets[round];
        if (!tarjandep.initialized)
        {
            tarjandep.strongConnect(round);
            lowLink = std::min(lowLink, tarjandep.lowLink);
        }
        else if (tarjandep.onStack)
        {
            lowLink = std::min(lowLink, tarjandep.nodeIndex);
        }
    }

    if (lowLink == nodeIndex)
    {
        vector<RealBTarget *> tempCycle;
        while (true)
        {
            RealBTarget *tarjanTemp = nodesStack.back();
            nodesStack.pop_back();
            tarjanTemp->onStack = false;
            tempCycle.emplace_back(tarjanTemp);
            if (tarjanTemp->bTarget == this->bTarget)
            {
                break;
            }
        }
        if (tempCycle.size() > 1)
        {
            for (const RealBTarget *c : tempCycle)
            {
                cycle.emplace_back(const_cast<BTarget *>(c->bTarget));
            }
            cycleExists = true;
            return;
        }
    }
    topologicalSort.emplace_back(const_cast<BTarget *>(bTarget));
}

void RealBTarget::checkForCycle()
{
    if (cycleExists)
    {
        string errorString = "There is a Cyclic-Dependency.\n";
        size_t cycleSize = cycle.size();
        for (unsigned int i = 0; i < cycleSize; ++i)
        {
            if (i == cycleSize - 1)
            {
                errorString +=
                    FORMAT("{} Depends On {}.\n", cycle[i]->getTarjanNodeName(), cycle[0]->getTarjanNodeName());
            }
            else
            {
                errorString +=
                    FORMAT("{} Depends On {}.\n", cycle[i]->getTarjanNodeName(), cycle[i + 1]->getTarjanNodeName());
            }
        }
        printErrorMessage(errorString);
        errorExit();
    }
}

RealBTarget::RealBTarget(BTarget *bTarget_, const unsigned short round_) : bTarget(bTarget_)
{
    uint32_t i;
    if (singleThreadRunning)
    {
        i = BTarget::tarjanNodesCount[round_].fetch_add(1);
    }
    else
    {
        i = BTarget::tarjanNodesCount[round_];
        ++BTarget::tarjanNodesCount[round_];
    }
    BTarget::tarjanNodesBTargets[round_][i] = this;
}

RealBTarget::RealBTarget(BTarget *bTarget_, const unsigned short round_, const bool add) : bTarget(bTarget_)
{
    if (add)
    {
        uint32_t i;
        if (singleThreadRunning)
        {
            i = BTarget::tarjanNodesCount[round_].fetch_add(1);
        }
        else
        {
            i = BTarget::tarjanNodesCount[round_];
            ++BTarget::tarjanNodesCount[round_];
        }
        BTarget::tarjanNodesBTargets[round_][i] = this;
    }
}

void RealBTarget::addInTarjanNodeBTarget(const unsigned short round_)
{
    uint32_t i;
    if (singleThreadRunning)
    {
        i = BTarget::tarjanNodesCount[round_].fetch_add(1);
    }
    else
    {
        i = BTarget::tarjanNodesCount[round_];
        ++BTarget::tarjanNodesCount[round_];
    }
    BTarget::tarjanNodesBTargets[round_][i] = this;
}

static string lowerCase(string str)
{
    lowerCasePStringOnWindows(str.data(), str.size());
    return str;
}

BTarget::BTarget() : realBTargets{RealBTarget(this, 0), RealBTarget(this, 1), RealBTarget(this, 2)}
{
    if (singleThreadRunning)
    {
        id = atomic_ref(total).fetch_add(1);
    }
    else
    {
        id = total;
        ++total;
    }
}

BTarget::BTarget(string name_, const bool buildExplicit_, bool makeDirectory)
    : realBTargets{RealBTarget(this, 0), RealBTarget(this, 1), RealBTarget(this, 2)}, name(lowerCase(std::move(name_))),
      buildExplicit(buildExplicit_)
{
    if (singleThreadRunning)
    {
        id = atomic_ref(total).fetch_add(1);
    }
    else
    {
        id = total;
        ++total;
    }
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (makeDirectory)
        {
            create_directory(configureNode->filePath + slashc + name);
        }
    }
    if (name.starts_with("conventional\\conventional\\"))
    {
        bool breakpoint = true;
    }
}

BTarget::BTarget(const bool add0, const bool add1, const bool add2)
    : realBTargets{RealBTarget(this, 0, add0), RealBTarget(this, 1, add1), RealBTarget(this, 2, add2)}
{
    if (singleThreadRunning)
    {
        id = atomic_ref(total).fetch_add(1);
    }
    else
    {
        id = total;
        ++total;
    }
}

BTarget::BTarget(string name_, const bool buildExplicit_, bool makeDirectory, const bool add0, const bool add1,
                 const bool add2)
    : realBTargets{RealBTarget(this, 0, add0), RealBTarget(this, 1, add1), RealBTarget(this, 2, add2)},
      name(lowerCase(std::move(name_))), buildExplicit(buildExplicit_)
{
    if (singleThreadRunning)
    {
        id = atomic_ref(total).fetch_add(1);
    }
    else
    {
        id = total;
        ++total;
    }
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (makeDirectory)
        {
            create_directory(configureNode->filePath + slashc + name);
        }
    }
    if (name.starts_with("conventional\\conventional\\"))
    {
        bool breakpoint = true;
    }
}

void BTarget::assignFileStatusToDependents(const unsigned short round)
{
    for (auto &[dependent, bTargetDepType] : realBTargets[round].dependents)
    {
        if (bTargetDepType == BTargetDepType::FULL)
        {
            atomic_ref(dependent->fileStatus).store(true);
        }
    }
}

void BTarget::receiveNotificationPostBuildSpecification()
{
    postBuildSpecificationArray.emplace_back(this);
}

void BTarget::runEndOfRoundTargets(Builder &builder, uint16_t round)
{
    if (round == 2)
    {
        delete[] tarjanNodesBTargets[round].data();
        for (auto *twoBTargetsVectorLocal : centralRegistryForTwoBTargetsVector)
        {
            for (auto [b, dep] : (*twoBTargetsVectorLocal)[1])
            {
                b->addDependencyNoMutex<1>(*dep);
            }
        }
    }
    else if (round == 1)
    {
        targetCacheDiskWriteManager.endOfRound();
        delete[] tarjanNodesBTargets[round].data();
        for (auto *twoBTargetsVectorLocal : centralRegistryForTwoBTargetsVector)
        {
            for (auto [b, dep] : (*twoBTargetsVectorLocal)[0])
            {
                b->addDependencyNoMutex<0>(*dep);
            }
        }
    }

    /*for (BTarget *t : roundEndTargets[round])
    {
        if (t != nullptr)
        {
            t->endOfRound(builder, round);
        }
    }*/
}

BTarget::~BTarget()
{
}

string BTarget::getTarjanNodeName() const
{
    return FORMAT("BTarget {}", id);
}

BTargetType BTarget::getBTargetType() const
{
    return BTargetType::DEFAULT;
}

void BTarget::updateBTarget(Builder &, unsigned short, bool &isComplete)
{
}

void BTarget::endOfRound(Builder &builder, unsigned short round)
{
}

bool operator<(const BTarget &lhs, const BTarget &rhs)
{
    return lhs.id < rhs.id;
}

// selectiveBuild is set for the children if hbuild is executed in parent dir. selectiveBuild is set for all
// targets that are present in cmdTargets. if a target explicitBuild is true, then it must be present in cmdTargets for
// its selectiveBuild to be true.
void BTarget::setSelectiveBuild()
{
    selectiveBuild = cmdTargets.contains(name);
    if (!buildExplicit && !selectiveBuild)
    {
        if (const uint64_t currentMinusConfigureSize = currentMinusConfigure.size())
        {
            const uint64_t nameSize = name.size();

            // Because name and crrentMinusConfigure don't end in slash lib3-cpp/ and lib3 will match. We do the
            // following check to avoid this
            if (nameSize < currentMinusConfigureSize)
            {
                if (currentMinusConfigure[nameSize] != slashc)
                {
                    return;
                }
            }
            else if (currentMinusConfigureSize < nameSize)
            {
                if (name[currentMinusConfigureSize] != slashc)
                {
                    return;
                }
            }

            if (const uint64_t minLength = std::min(nameSize, currentMinusConfigureSize))
            {
                // Compare characters up to the shorter length
                bool mismatch = false;
                for (uint64_t i = 0; i < minLength; ++i)
                {
                    if (name[i] != currentMinusConfigure[i])
                    {
                        mismatch = true;
                    }
                }

                if (!mismatch)
                {
                    selectiveBuild = true;
                }
            }
        }
        else
        {
            selectiveBuild = true;
        }
    }
}

// Returns true if hbuild is executed in the same dir or the child dir. Used in hmake.cpp to rule out other
// configurations specifications
bool BTarget::isHBuildInSameOrChildDirectory() const
{
    return childInParentPathNormalized(configureNode->filePath + slashc + name, currentNode->filePath);
}