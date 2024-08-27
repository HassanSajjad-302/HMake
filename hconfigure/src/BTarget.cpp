
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
        tarjanNodesBTargets.emplace_back();
        tarjanNodesBTargetsMutexes.emplace_back(new mutex());
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
    std::lock_guard lk(*tarjanNodesBTargetsMutexes[round]);

    // Memory Not Released
    tarjanNodesBTargets[round].emplace_back(this);
}

BTarget::BTarget() : realBTargets{RealBTarget(this, 0), RealBTarget(this, 1), RealBTarget(this, 2)}
{
    id = total++;
}

static pstring lowerCase(pstring str)
{
    lowerCasePStringOnWindows(const_cast<pchar *>(str.c_str()), str.size());
    return str;
}

BTarget::BTarget(pstring name_, bool buildExplicit, bool makeDirectory)
    : realBTargets{RealBTarget(this, 0), RealBTarget(this, 1), RealBTarget(this, 2)},
      name(lowerCase(std::move(name_)))
{
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