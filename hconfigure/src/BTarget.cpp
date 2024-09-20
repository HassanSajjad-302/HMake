
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
using std::filesystem::create_directories, std::ofstream, std::filesystem::current_path, std::mutex, std::lock_guard;

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
    return const_cast<BTarget *>(lhs)->realBTargets[0].indexInTopologicalSort >
           const_cast<BTarget *>(rhs)->realBTargets[0].indexInTopologicalSort;
}

bool IndexInTopologicalSortComparatorRoundTwo::operator()(const BTarget *lhs, const BTarget *rhs) const
{
    return const_cast<BTarget *>(lhs)->realBTargets[2].indexInTopologicalSort >
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

static pstring lowerCase(pstring str)
{
    lowerCasePStringOnWindows(const_cast<pchar *>(str.c_str()), str.size());
    return str;
}

BTarget::BTarget() : realBTargets{RealBTarget(this, 0), RealBTarget(this, 1), RealBTarget(this, 2)}
{
    id = atomic_ref(total).fetch_add(1);
}
BTarget::BTarget(pstring name_, bool buildExplicit, bool makeDirectory)
    : realBTargets{RealBTarget(this, 0), RealBTarget(this, 1), RealBTarget(this, 2)}, name(lowerCase(std::move(name_)))
{
    id = atomic_ref(total).fetch_add(1);
}

BTarget::BTarget(const bool add0, const bool add1, const bool add2)
    : realBTargets{RealBTarget(this, 0, add0), RealBTarget(this, 1, add1), RealBTarget(this, 2, add2)}
{
    id = atomic_ref(total).fetch_add(1);
}

BTarget::BTarget(pstring name_, bool buildExplicit, bool makeDirectory, const bool add0, const bool add1,
                 const bool add2)
    : realBTargets{RealBTarget(this, 0, add0), RealBTarget(this, 1, add1), RealBTarget(this, 2, add2)},
      name(lowerCase(std::move(name_)))
{
    id = atomic_ref(total).fetch_add(1);
}

BTarget::~BTarget()
{
}

pstring BTarget::getTarjanNodeName() const
{
    return "BTarget " + to_pstring(id);
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

bool operator<(const BTarget &lhs, const BTarget &rhs)
{
    return lhs.id < rhs.id;
}

// selectiveBuild is set for the children if hbuild is executed in parent directory. Uses by the Builder::Builder
void BTarget::setSelectiveBuild()
{
    selectiveBuild =
        childInParentPathRecursiveNormalized(currentNode->filePath, configureNode->filePath + slashc + name);
}

// selectiveBuild is set for the parent if hbuild is executed in child directory. Used in hmake.cpp to rule out other
// configurations specifications
bool BTarget::getSelectiveBuildChildDir()
{
    return childInParentPathRecursiveNormalized(configureNode->filePath + slashc + name, currentNode->filePath);
}