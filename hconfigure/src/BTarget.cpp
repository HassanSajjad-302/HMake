#include "BTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppMod.hpp"
#include "rapidhash/rapidhash.h"
#include <filesystem>
#include <utility>

using std::filesystem::create_directories, std::ofstream, std::filesystem::current_path, std::lock_guard,
    std::filesystem::create_directory;

bool IndexInTopologicalSortComparatorRoundZero::operator()(const BTarget *lhs, const BTarget *rhs) const
{
    return const_cast<BTarget *>(lhs)->realBTargets[0].indexInTopologicalSort <
           const_cast<BTarget *>(rhs)->realBTargets[0].indexInTopologicalSort;
}

bool IndexInTopologicalSortComparatorRoundTwo::operator()(const BTarget *lhs, const BTarget *rhs) const
{
    return const_cast<BTarget *>(lhs)->realBTargets[1].indexInTopologicalSort <
           const_cast<BTarget *>(rhs)->realBTargets[1].indexInTopologicalSort;
}

void RealBTarget::sortGraph()
{
    if (graphEdges.empty())
    {
        sorted.clear();
        cycleExists = false;
        return;
    }

    vector<RealBTarget *> noEdges;
    uint32_t noEdgesCount = 0;

    sorted.clear();
    sorted.resize(graphEdges.size());
    cycleExists = false;

    uint32_t edgesCount = 0;
    uint32_t index = graphEdges.size() - 1;
    for (RealBTarget *r : graphEdges)
    {
        r->indexInTopologicalSort = r->dependents.size();
        if (!r->indexInTopologicalSort)
        {
            noEdges.emplace_back(r);
        }
        edgesCount += r->indexInTopologicalSort;
    }

    while (noEdges.size() != noEdgesCount)
    {
        RealBTarget *rb = noEdges[noEdgesCount];
        sorted[index] = rb;
        --index;
        for (const RBTWithType &rbt : rb->dependencies)
        {
            --edgesCount;
            if (!--rbt.getPointer()->indexInTopologicalSort)
            {
                noEdges.emplace_back(rbt.getPointer());
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
            if (r->indexInTopologicalSort > 0)
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
    }
}

bool RealBTarget::findCycleDFS(RealBTarget *node, flat_hash_set<RealBTarget *> &visited,
                               flat_hash_set<RealBTarget *> &recursionStack, vector<RealBTarget *> &currentPath,
                               string &errorString)
{
    visited.insert(node);
    recursionStack.insert(node);
    currentPath.push_back(node);

    // Only consider dependencies that are still part of the cycle
    for (const RBTWithType &rbt : node->dependencies)
    {
        // Only follow edges to nodes that are part of the cycle
        if (rbt.getPointer()->indexInTopologicalSort > 0)
        {
            if (recursionStack.find(rbt.getPointer()) != recursionStack.end())
            {
                // Found a cycle! Print the path from dependency back to current node
                if (auto cycleStart = find(currentPath.begin(), currentPath.end(), rbt.getPointer());
                    cycleStart != currentPath.end())
                {
                    errorString += "Cycle found: ";
                    for (auto it = cycleStart; it != currentPath.end(); ++it)
                    {
                        errorString += (*it)->getBTarget()->getPrintName() + " -> ";
                    }
                    errorString += rbt.getPointer()->getBTarget()->getPrintName() + "\n";
                    return true;
                }
            }
            else if (visited.find(rbt.getPointer()) == visited.end())
            {
                if (findCycleDFS(rbt.getPointer(), visited, recursionStack, currentPath, errorString))
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

void RealBTarget::printSortedGraph()
{
    for (const RealBTarget *rb : sorted)
    {
        printMessage(rb->getBTarget()->getPrintName() + '\n');
    }
    fflush(stdout);
}

RealBTarget::RealBTarget(const unsigned short round_) : round(round_)
{
    uint32_t i = BTarget::realBTargetsArrayCount[round];
    ++BTarget::realBTargetsArrayCount[round];
    BTarget::realBTargetsGlobal[round][i] = this;
}

RealBTarget::RealBTarget(const unsigned short round_, const bool add) : round(round_)
{
    if (add)
    {
        const uint32_t i = BTarget::realBTargetsArrayCount[round];
        ++BTarget::realBTargetsArrayCount[round];
        BTarget::realBTargetsGlobal[round][i] = this;
    }
}

bool RealBTarget::checkDepsChanged() const
{
    const char *ptr = bTargetCaches[getBTarget()->cacheIndex].depsCache.data();
    uint32_t bytesRead = 0;
    const uint32_t cachedCount = readUint32(ptr, bytesRead);

    if (cachedCount != dependenciesSize)
    {
        return true;
    }

    for (uint32_t i = 0; i < cachedCount; ++i)
    {
        BTarget *bt = bTargetCaches[readUint32(ptr, bytesRead)].bTarget;
        if (!bt || !dependencies.contains(&bt->realBTargets[0]))
        {
            return true;
        }
    }

    return false;
}

void RealBTarget::getAllWaitDepsTopological(
    btree_set<BTarget *, IndexInTopologicalSortComparatorRoundZero> &allDepsTransitive)
{
    for (const RBTWithType &rbt : dependencies)
    {
        if (rbt.getRelationType() == RelationType::FULL || rbt.getRelationType() == RelationType::WAIT)
        {
            BTarget *bt = rbt.getPointer()->getBTarget();
            if (allDepsTransitive.emplace(bt).second)
            {
                rbt.getPointer()->getAllWaitDepsTopological(allDepsTransitive);
            }
        }
    }
}

static string lowerCase(string str)
{
    lowerCaseOnWindows(str.data(), str.size());
    return str;
}

namespace
{
uint64_t idCount = 0;
flat_hash_map<BTarget *, uint64_t> bTargetIndexAndMyIdHashMap;
uint64_t myId = 0;

void checkForSameTargetName(BTarget *bTarget, const uint64_t targetName)
{
    myId = idCount;
    ++idCount;
    if (auto [pos, ok] = bTargetIndexAndMyIdHashMap.emplace(bTarget, targetName); !ok)
    {
        printErrorMessage(FORMAT("Attempting to add 2 targets\n{}\n{} with same cache-name {} in config-cache.json\n",
                                 pos->first->getPrintName(), bTarget->getPrintName(), targetName));
        errorExit();
    }
}
} // namespace

void BTarget::initializeBTarget(bool makeDirectory)
{
    id = total;
    ++total;
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (makeDirectory)
        {
            create_directory(configureNode->filePath + slashc + name);
        }
    }

    const auto it = nameToIndexMap.find(cacheName);
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (it == nameToIndexMap.end())
        {
            cacheIndex = bTargetCaches.size();
            bTargetCaches.emplace_back().name = cacheName;
            newlyAdded = true;
        }
        else
        {
            cacheIndex = it->second;
        }

        checkForSameTargetName(this, cacheName);
    }
    else
    {
        if (it == nameToIndexMap.end())
        {
            printErrorMessage(FORMAT("Target\nname: {}\ncacheName: {}\n not found in config-cache.\nMaybe you need to "
                                     "run hhelper first to update the target-cache.\n",
                                     name, cacheName));
        }
        cacheIndex = it->second;

        if (launchesProcess)
        {
            RealBTarget &rb = realBTargets[0];
            const char *ptr = bTargetCaches[cacheIndex].getBuildFooter().data();
            uint32_t bytesRead = 8;
            rb.launchTime = readUint64(ptr, bytesRead);
        }
    }
    bTargetCaches[cacheIndex].bTarget = this;
}

BTarget::BTarget(string name_, const bool launchesProcess_, const BTargetType type_)
    : name(lowerCase(std::move(name_))), cacheName(rapidhash(name.data(), name.size())), bTargetType(type_),
      launchesProcess(launchesProcess_), realBTargets{RealBTarget(0), RealBTarget(1)}
{
    initializeBTarget(false);
}

BTarget::BTarget(string name_, const bool launchesProcess_, const BTargetType type_, const bool buildExplicit_,
                 bool makeDirectory)
    : name(lowerCase(std::move(name_))), cacheName(rapidhash(name.data(), name.size())), bTargetType(type_),
      launchesProcess(launchesProcess_), buildExplicit(buildExplicit_), realBTargets{RealBTarget(0), RealBTarget(1)}
{
    initializeBTarget(makeDirectory);
}

BTarget::BTarget(string name_, const bool launchesProcess_, const BTargetType type_, const bool buildExplicit_,
                 bool makeDirectory, const bool add0, const bool add1)
    : name(lowerCase(std::move(name_))), cacheName(rapidhash(name.data(), name.size())), bTargetType(type_),
      launchesProcess(launchesProcess_), buildExplicit(buildExplicit_),
      realBTargets{RealBTarget(0, add0), RealBTarget(1, add1)}
{
    initializeBTarget(makeDirectory);
}

BTarget::BTarget(string name_, const uint64_t cacheName_, const bool launchesProcess_, const BTargetType type_)
    : name(lowerCase(std::move(name_))), cacheName(cacheName_), bTargetType(type_), launchesProcess(launchesProcess_),
      realBTargets{RealBTarget(0), RealBTarget(1)}
{
    initializeBTarget(false);
}

BTarget::BTarget(string name_, const uint64_t cacheName_, const bool launchesProcess_, const BTargetType type_,
                 const bool buildExplicit_, bool makeDirectory)
    : name(lowerCase(std::move(name_))), cacheName(cacheName_), bTargetType(type_), launchesProcess(launchesProcess_),
      buildExplicit(buildExplicit_), realBTargets{RealBTarget(0), RealBTarget(1)}
{
    initializeBTarget(makeDirectory);
}

BTarget::BTarget(string name_, const uint64_t cacheName_, const bool launchesProcess_, const BTargetType type_,
                 const bool buildExplicit_, bool makeDirectory, const bool add0, const bool add1)
    : name(lowerCase(std::move(name_))), cacheName(cacheName_), bTargetType(type_), launchesProcess(launchesProcess_),
      buildExplicit(buildExplicit_), realBTargets{RealBTarget(0, add0), RealBTarget(1, add1)}
{
    initializeBTarget(makeDirectory);
}

BTarget::~BTarget()
{
}

void BTarget::writeBuildCacheHeaderAtBuildTime(string &buffer) const
{
    writeUint64(buffer, realBTargets[0].cumulativeHash);
    writeUint64(buffer, realBTargets[0].launchTime);
}

string BTarget::getPrintName() const
{
    if (!name.empty())
    {
        return name;
    }
    return FORMAT("BTarget {}", id);
}

void BTarget::completeRoundOne()
{
}

bool BTarget::isEventRegistered(Builder &builder)
{
    return false;
}

bool BTarget::isEventCompleted(Builder &builder, string_view message)
{
    return false;
}

void BTarget::setUpdateStatus()
{
    RealBTarget &rb = realBTargets[0];
    if (rb.updateStatus != UpdateStatus::UNCHECKED)
    {
       return;
    }

    uint64_t highestTime;
    if (launchesProcess)
    {
        uint32_t bytesRead = 0;
        const char *ptr = bTargetCaches[cacheIndex].getBuildFooter().data();
        if (const uint64_t compileHash = readUint64(ptr, bytesRead); compileHash != rb.cumulativeHash)
        {
            rb.updateStatus = UpdateStatus::UPDATE_NEEDED;
            return;
        }
        highestTime = rb.launchTime;
    }
    else
    {
        highestTime = 0;
    }

    for (const RBTWithType &rbt : rb.dependencies)
    {
        if (rbt.getRelationType() == RelationType::FULL || rbt.getRelationType() == RelationType::WAIT)
        {
        }
        else
        {
            continue;
        }
        const RealBTarget *depRb = rbt.getPointer();

        if (depRb->updateStatus == UpdateStatus::UNCHECKED)
        {
            depRb->getBTarget()->setUpdateStatus();
        }

        if (depRb->updateStatus == UpdateStatus::UPDATE_NEEDED)
        {
            rb.updateStatus = UpdateStatus::UPDATE_NEEDED;
            return;
        }

        if (depRb->launchTime > highestTime)
        {
            if (launchesProcess)
            {
                rb.updateStatus = UpdateStatus::UPDATE_NEEDED;
                return;
            }
            highestTime = depRb->launchTime;
        }
    }

    rb.updateStatus = UpdateStatus::UPDATE_NOT_NEEDED;

    if (!launchesProcess)
    {
        // highest time is set as the highest time of one of our dependencies.
        rb.launchTime = highestTime;
    }
}

void BTarget::generateStandAloneCommand()
{
}

void BTarget::cppStandAloneCommand(flat_hash_set<string> &createdDirs, string &scriptContents, const string &scriptDir,
                                   bool direct)
{
}

void BTarget::writeConfigCacheAtConfigTime(string &buffer)
{
}

void BTarget::writeBuildCacheAtBuildTime(string &buffer)
{
    HMAKE_HMAKE_INTERNAL_ERROR
}

void BTarget::verifyBuildCache(string_view buildCache) const
{
    if (launchesProcess)
    {
        if (buildCache.size() != 16)
        {
            printErrorMessage(
                FORMAT("{} caching-verification failed as buildCache size is not equal to 16.\n", getPrintName()));
        }
        uint32_t bytesRead = 0;
        verifyBTargetHeader(buildCache, bytesRead);
    }
}

void BTarget::verifyBTargetHeader(string_view buildCache, uint32_t &bytesRead) const
{
    if (newlyAdded)
    {
        bytesRead += 16;
        return;
    }
    if (buildCache.size() < 16)
    {
        printErrorMessage(
            FORMAT("{} caching-verification failed as buildCache size is less than 16.\n", getPrintName()));
    }
    if (const uint64_t commandHash = readUint64(buildCache.data(), bytesRead);
        commandHash != realBTargets[0].cumulativeHash)
    {
        printErrorMessage(
            FORMAT("{} caching-verification failed as command-hashes don't reconcile.\ncached vs current: {} vs {}",
                   getPrintName(), commandHash, realBTargets[0].cumulativeHash));
    }
    if (const uint64_t launchTime = readUint64(buildCache.data(), bytesRead); launchTime != realBTargets[0].launchTime)
    {
        printErrorMessage(
            FORMAT("{} caching-verification failed as launch-times don't reconcile.\ncached vs current: {} vs {}",
                   getPrintName(), launchTime, realBTargets[0].launchTime));
    }
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

bool readBool(const char *ptr, uint32_t &bytesRead)
{
    bool result;
    memcpy(&result, ptr + bytesRead, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

uint8_t readUint8(const char *ptr, uint32_t &bytesRead)
{
    uint8_t result;
    memcpy(&result, ptr + bytesRead, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

uint32_t readUint32(const char *ptr, uint32_t &bytesRead)
{
    uint32_t result;
    memcpy(&result, ptr + bytesRead, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

uint64_t readUint64(const char *ptr, uint32_t &bytesRead)
{
    uint64_t result;
    memcpy(&result, ptr + bytesRead, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

string_view readStringView(const char *ptr, uint32_t &bytesRead)
{
    uint32_t strSize = readUint32(ptr, bytesRead);
    const uint32_t offset = bytesRead;
    bytesRead += strSize;
    return {ptr + offset, strSize};
}

Node *readHalfNode(const char *ptr, uint32_t &bytesRead)
{
    uint32_t strSize = readUint32(ptr, bytesRead);
    return nodeIndices[strSize];
}

void writeBool(string &buffer, const bool &value)
{
    buffer.push_back(value);
}

void writeUint8(string &buffer, const uint8_t &data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.append(ptr, ptr + sizeof(data));
}

void writeUint32(string &buffer, const uint32_t data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.append(ptr, ptr + sizeof(data));
}

void writeUint64(string &buffer, const uint64_t data)
{
    const auto ptr = reinterpret_cast<const char *>(&data);
    buffer.append(ptr, ptr + sizeof(data));
}

void writeStringView(string &buffer, const string_view &data)
{
    writeUint32(buffer, data.size());
    buffer.append(data.begin(), data.end());
}

void writeNode(string &buffer, const Node *node)
{
    writeUint32(buffer, node->myId);
}

void writeNodeVector(string &buffer, const vector<Node *> &array)
{
    writeUint32(buffer, array.size());
    for (auto &e : array)
    {
        writeNode(buffer, e);
    }
}