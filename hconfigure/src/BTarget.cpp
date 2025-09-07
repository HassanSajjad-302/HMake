
#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
import "BuildSystemFunctions.hpp";
import "CppSourceTarget.hpp";
import "CacheWriteManager.hpp";
import <filesystem>;
import <utility>;
#else
#include "BTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "CacheWriteManager.hpp"
#include "CppSourceTarget.hpp"
#include <filesystem>
#include <utility>
#endif
using std::filesystem::create_directories, std::ofstream, std::filesystem::current_path, std::mutex, std::lock_guard,
    std::filesystem::create_directory;

BTarget::LaterDep::LaterDep(RealBTarget *b_, RealBTarget *dep_, BTargetDepType type_, bool doBoth_)
    : b(b_), dep(dep_), type(type_), doBoth(doBoth_)
{
}

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

void RealBTarget::sortGraph()
{
    vector<RealBTarget *> noEdges;
    uint32_t noEdgesCount = 0;

    sorted.clear();
    sorted.resize(graphEdges.size());
    cycleExists = false;

    uint32_t edgesCount = 0;
    uint32_t index = graphEdges.size() - 1;
    for (RealBTarget *r : graphEdges)
    {
        r->dependentsCount = r->dependents.size();
        if (!r->dependentsCount)
        {
            noEdges.emplace_back(r);
        }
        edgesCount += r->dependentsCount;
    }

    while (noEdges.size() != noEdgesCount)
    {
        RealBTarget *rb = noEdges[noEdgesCount];
        sorted[index] = rb;
        --index;
        for (auto [m, _] : rb->dependencies)
        {
            --edgesCount;
            if (!--m->dependentsCount)
            {
                noEdges.emplace_back(m);
            }
        }
        ++noEdgesCount;
    }

    if (edgesCount)
    {
        cycleExists = true;

        // Find all nodes that are part of cycles
        // These are nodes that still have dependentsCount > 0
        for (RealBTarget *r : graphEdges)
        {
            if (r->dependentsCount > 0)
            {
                cycle.emplace_back(r);
            }
        }

        string errorString;

        // This function finds actual cycles using DFS from nodes still in the graph
        flat_hash_set<RealBTarget *> visited;
        flat_hash_set<RealBTarget *> recursionStack;
        vector<RealBTarget *> currentPath;

        for (RealBTarget *node : cycle)
        {
            if (visited.find(node) == visited.end())
            {
                if (findCycleDFS(node, visited, recursionStack, currentPath, errorString))
                {
                    break; // Found one cycle, that's enough for error reporting
                }
            }
        }

        printErrorMessage(errorString);
        errorExit();
    }
}

void RealBTarget::printSortedGraph()
{
    for (RealBTarget *rb : sorted)
    {
        printMessage(rb->bTarget->getPrintName() + '\n');
    }
    fflush(stdout);
}

bool RealBTarget::findCycleDFS(RealBTarget *node, flat_hash_set<RealBTarget *> &visited,
                               flat_hash_set<RealBTarget *> &recursionStack, vector<RealBTarget *> &currentPath,
                               string &errorString)
{
    visited.insert(node);
    recursionStack.insert(node);
    currentPath.push_back(node);

    // Only consider dependencies that are still part of the cycle
    for (auto [dependency, _] : node->dependencies)
    {
        // Only follow edges to nodes that are part of the cycle
        if (dependency->dependentsCount > 0)
        {
            if (recursionStack.find(dependency) != recursionStack.end())
            {
                // Found a cycle! Print the path from dependency back to current node
                auto cycleStart = find(currentPath.begin(), currentPath.end(), dependency);
                if (cycleStart != currentPath.end())
                {
                    errorString += "Cycle found: ";
                    for (auto it = cycleStart; it != currentPath.end(); ++it)
                    {
                        errorString += (*it)->bTarget->getPrintName() + " -> ";
                    }
                    errorString += dependency->bTarget->getPrintName() + "\n";
                    return true;
                }
            }
            else if (visited.find(dependency) == visited.end())
            {
                if (findCycleDFS(dependency, visited, recursionStack, currentPath, errorString))
                {
                    return true;
                }
            }
        }
    }

    recursionStack.erase(node);
    currentPath.pop_back();
    return false;
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

void RealBTarget::assignFileStatusToDependents()
{
    for (auto &[dependent, bTargetDepType] : dependents)
    {
        if (bTargetDepType == BTargetDepType::FULL)
        {
            dependent->updateStatus = UpdateStatus::NEEDS_UPDATE;
        }
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

BTarget::BTarget() : realBTargets{RealBTarget(this, 0), RealBTarget(this, 1)}
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
    : realBTargets{RealBTarget(this, 0), RealBTarget(this, 1)}, name(lowerCase(std::move(name_))),
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

BTarget::BTarget(const bool add0, const bool add1)
    : realBTargets{RealBTarget(this, 0, add0), RealBTarget(this, 1, add1)}
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

BTarget::BTarget(string name_, const bool buildExplicit_, bool makeDirectory, const bool add0, const bool add1)
    : realBTargets{RealBTarget(this, 0, add0), RealBTarget(this, 1)}, name(lowerCase(std::move(name_))),
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

void BTarget::receiveNotificationPostBuildSpecification()
{
    postBuildSpecificationArray.emplace_back(this);
}

void BTarget::runEndOfRoundTargets()
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        cacheWriteManager.endOfRound();
    }

    delete[] tarjanNodesBTargets[1].data();

    for (vector<LaterDep> *laterDeps : laterDepsCentral)
    {
        for (LaterDep &later : *laterDeps)
        {
            // first emplace in dependents, if ok, then emplace in dependencies based on doBoth variable.
            if (later.dep->dependents.try_emplace(later.b, later.type).second)
            {
                if (later.doBoth)
                {
                    later.b->dependencies.emplace(later.dep, later.type);
                    if (later.type == BTargetDepType::FULL)
                    {
                        ++later.b->dependenciesSize;
                    }
                }
            }
        }
    }
}

BTarget::~BTarget()
{
}

string BTarget::getPrintName() const
{
    if (!name.empty())
    {
        return name;
    }
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

void BTarget::addDepHalfNowHalfLater(BTarget &dep)
{
    if (realBTargets[0].dependencies.try_emplace(&dep.realBTargets[0], BTargetDepType::FULL).second)
    {
        ++realBTargets[0].dependenciesSize;
        laterDepsLocal.emplace_back(&this->realBTargets[0], &dep.realBTargets[0], BTargetDepType::FULL, false);
    }
}

void BTarget::addDepLooseHalfNowHalfLater(BTarget &dep)
{
    if (realBTargets[0].dependencies.try_emplace(&dep.realBTargets[0], BTargetDepType::LOOSE).second)
    {
        laterDepsLocal.emplace_back(&this->realBTargets[0], &dep.realBTargets[0], BTargetDepType::LOOSE, false);
    }
}

void BTarget::addDepLater(BTarget &dep)
{
    laterDepsLocal.emplace_back(&this->realBTargets[0], &dep.realBTargets[0], BTargetDepType::FULL, true);
}

void BTarget::addDepLooseLater(BTarget &dep)
{
    laterDepsLocal.emplace_back(&this->realBTargets[0], &dep.realBTargets[0], BTargetDepType::LOOSE, true);
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