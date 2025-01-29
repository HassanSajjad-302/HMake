
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

RealBTarget::RealBTarget(BTarget *bTarget_, const unsigned short round_)
    : TBT{bTarget_}, bTarget(bTarget_), round(round_)
{
    const uint32_t i = atomic_ref(tarjanNodesCount[round]).fetch_add(1);
    tarjanNodesBTargets[round][i] = this;
}

RealBTarget::RealBTarget(BTarget *bTarget_, const unsigned short round_, const bool add)
    : TBT{bTarget_}, bTarget(bTarget_), round(round_)
{
    if (add)
    {
        const uint32_t i = atomic_ref(tarjanNodesCount[round]).fetch_add(1);
        tarjanNodesBTargets[round][i] = this;
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

void BTarget::assignFileStatusToDependents(RealBTarget &realBTarget)
{
    for (auto &[dependent, bTargetDepType] : realBTarget.dependents)
    {
        if (bTargetDepType == BTargetDepType::FULL)
        {
            atomic_ref(dependent->fileStatus).store(true);
        }
    }
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

bool operator<(const BTarget &lhs, const BTarget &rhs)
{
    return lhs.id < rhs.id;
}

// selectiveBuild is set for the children if hbuild is executed in parent directory. cmdTargets augments the
// selectiveBuild targets especially for targets whose buildExplicit is true.
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

// selectiveBuild is set for the parent if hbuild is executed in child directory. Used in hmake.cpp to rule out other
// configurations specifications
bool BTarget::getSelectiveBuildChildDir()
{
    return childInParentPathNormalized(configureNode->filePath + slashc + name, currentNode->filePath);
}