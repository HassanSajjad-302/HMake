
#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
import "Builder.hpp";
import "BuildSystemFunctions.hpp";
import "CppSourceTarget.hpp";
import <filesystem>;
import <fstream>;
#else
#include "BTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "CppSourceTarget.hpp"
#include <filesystem>
#include <fstream>
#include <utility>
#endif
using std::filesystem::create_directories, std::ofstream, std::filesystem::current_path, std::mutex, std::lock_guard,
    std::filesystem::create_directory;

StaticInitializationTarjanNodesBTargets::StaticInitializationTarjanNodesBTargets()
{
    for (unsigned short i = 0; i < 3; ++i)
    {
        tarjanNodesBTargets.emplace_back(10000);
        tarjanNodesCount.emplace_back(0);
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
    for (RealBTarget *i : *tarjanNodes)
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
    for (RealBTarget *tarjanNode : *tarjanNodes)
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
        printErrorMessageColor("There is a Cyclic-Dependency.\n", settings.pcSettings.toolErrorOutput);
        size_t cycleSize = cycle.size();
        for (unsigned int i = 0; i < cycleSize; ++i)
        {
            if (i == cycleSize - 1)
            {
                printErrorMessageColor(
                    FORMAT("{} Depends On {}.\n", cycle[i]->getTarjanNodeName(), cycle[0]->getTarjanNodeName()),
                    settings.pcSettings.toolErrorOutput);
            }
            else
            {
                printErrorMessageColor(
                    FORMAT("{} Depends On {}.\n", cycle[i]->getTarjanNodeName(), cycle[i + 1]->getTarjanNodeName()),
                    settings.pcSettings.toolErrorOutput);
            }
        }
        exit(EXIT_FAILURE);
    }
}

RealBTarget::RealBTarget(BTarget *bTarget_, const unsigned short round_) : bTarget(bTarget_)
{
    const uint32_t i = atomic_ref(tarjanNodesCount[round_]).fetch_add(1);
    tarjanNodesBTargets[round_][i] = this;
}

RealBTarget::RealBTarget(BTarget *bTarget_, const unsigned short round_, const bool add) : bTarget(bTarget_)
{
    if (add)
    {
        const uint32_t i = atomic_ref(tarjanNodesCount[round_]).fetch_add(1);
        tarjanNodesBTargets[round_][i] = this;
    }
}

void RealBTarget::addInTarjanNodeBTarget(const unsigned short round_)
{
    const uint32_t i = atomic_ref(tarjanNodesCount[round_]).fetch_add(1);
    tarjanNodesBTargets[round_][i] = this;
}

static string lowerCase(string str)
{
    lowerCasePStringOnWindows(str.data(), str.size());
    return str;
}

BTarget::BTarget() : realBTargets{RealBTarget(this, 0), RealBTarget(this, 1), RealBTarget(this, 2)}
{
    id = atomic_ref(total).fetch_add(1);
}

BTarget::BTarget(string name_, const bool buildExplicit_, bool makeDirectory)
    : realBTargets{RealBTarget(this, 0), RealBTarget(this, 1), RealBTarget(this, 2)}, name(lowerCase(std::move(name_))),
      buildExplicit(buildExplicit_)
{
    id = atomic_ref(total).fetch_add(1);
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
    id = atomic_ref(total).fetch_add(1);
}

BTarget::BTarget(string name_, const bool buildExplicit_, bool makeDirectory, const bool add0, const bool add1,
                 const bool add2)
    : realBTargets{RealBTarget(this, 0, add0), RealBTarget(this, 1, add1), RealBTarget(this, 2, add2)},
      name(lowerCase(std::move(name_))), buildExplicit(buildExplicit_)
{
    id = atomic_ref(total).fetch_add(1);
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

BTarget::~BTarget()
{
}

string BTarget::getTarjanNodeName() const
{
    return FORMAT("BTarget {}", id);
}

BTargetType BTarget::getBTargetType() const
{
    return static_cast<BTargetType>(0);
}

void BTarget::updateBTarget(Builder &, unsigned short)
{
}

void BTarget::endOfRound(Builder &builder, unsigned short round)
{
}

void BTarget::copyJson()
{
}

void BTarget::buildSpecificationCompleted()
{
}

bool operator<(const BTarget &lhs, const BTarget &rhs)
{
    return lhs.id < rhs.id;
}

// selectiveBuild is set for the children if hbuild is executed in parent directory. selectiveBuild is set for all
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

// Returns true if hbuild is executed in the same directory or the child directory. Used in hmake.cpp to rule out other
// configurations specifications
bool BTarget::isHBuildInSameOrChildDirectory() const
{
    return childInParentPathNormalized(configureNode->filePath + slashc + name, currentNode->filePath);
}