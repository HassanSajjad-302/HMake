#include "BTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppMod.hpp"
#include "rapidhash/rapidhash.h"
#include <filesystem>
#include <utility>

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
    cycle.clear();
    if (graphEdges.empty())
    {
        sorted.clear();
        cycleExists = false;
        return;
    }

    // Independent targets deliberately keep discovery order. Required ordering belongs in dependency edges, while
    // this append-only frontier keeps Kahn's traversal O(V + E).
    STACK_PMR_VECTOR(RealBTarget *, noEdges, 32 * 1024)
    noEdges.reserve(graphEdges.size());
    uint64_t noEdgesIndex = 0;

    sorted.clear();
    sorted.resize(graphEdges.size());
    cycleExists = false;

    uint64_t edgesCount = 0;
    uint64_t remaining = graphEdges.size();
    for (RealBTarget *r : graphEdges)
    {
        constexpr uint64_t maxPackedTopologicalIndex = (uint64_t{1} << 29) - 1;
        if (r->dependents.size() > maxPackedTopologicalIndex)
        {
            printErrorMessage(FORMAT("Build target has too many dependents.\nTarget: {}\nDependents: {}\nLimit: {}",
                                     r->getBTarget()->getPrintName(), r->dependents.size(), maxPackedTopologicalIndex));
        }
        r->indexInTopologicalSort = r->dependents.size();
        if (!r->indexInTopologicalSort)
        {
            noEdges.emplace_back(r);
        }
        edgesCount += r->indexInTopologicalSort;
    }

    while (noEdgesIndex != noEdges.size())
    {
        RealBTarget *rb = noEdges[noEdgesIndex++];
        sorted[--remaining] = rb;
        for (const RBTWithType &rbt : rb->dependencies)
        {
            --edgesCount;
            if (!--rbt.getPointer()->indexInTopologicalSort)
            {
                noEdges.emplace_back(rbt.getPointer());
            }
        }
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
        std::ranges::sort(cycle, [](const RealBTarget *lhs, const RealBTarget *rhs) {
            return lhs->getBTarget()->id < rhs->getBTarget()->id;
        });

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

    vector<RealBTarget *> remainingDependencies;
    remainingDependencies.reserve(node->dependencies.size());
    for (const RBTWithType &rbt : node->dependencies)
    {
        if (rbt.getPointer()->indexInTopologicalSort > 0)
        {
            remainingDependencies.emplace_back(rbt.getPointer());
        }
    }
    std::ranges::sort(remainingDependencies, [](const RealBTarget *lhs, const RealBTarget *rhs) {
        return lhs->getBTarget()->id < rhs->getBTarget()->id;
    });

    // Only consider dependencies that remain after Kahn's algorithm.
    for (RealBTarget *dependency : remainingDependencies)
    {
        if (recursionStack.find(dependency) != recursionStack.end())
        {
            // Found a cycle. Print the path from dependency back to the current node.
            if (auto cycleStart = find(currentPath.begin(), currentPath.end(), dependency);
                cycleStart != currentPath.end())
            {
                errorString = "Dependency graph contains a cycle.\nCycle: ";
                for (auto it = cycleStart; it != currentPath.end(); ++it)
                {
                    errorString += (*it)->getBTarget()->getPrintName() + " -> ";
                }
                errorString += dependency->getBTarget()->getPrintName() + "\n";
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

namespace
{
void registerRealBTarget(RealBTarget *target, const unsigned short round)
{
    if (round >= BTarget::realBTargetsGlobal.size())
    {
        printErrorMessage(FORMAT("Invalid build-graph round.\nRound: {}\nValid rounds: 0 and 1", round));
    }

    const uint32_t index = BTarget::realBTargetsArrayCount[round];
    if (index >= BTarget::realBTargetsGlobal[round].size())
    {
        printErrorMessage(FORMAT("Maximum build-target count exceeded.\nRound: {}\nLimit: {}", round,
                                 BTarget::realBTargetsGlobal[round].size()));
    }

    BTarget::realBTargetsGlobal[round][index] = target;
    ++BTarget::realBTargetsArrayCount[round];
}
} // namespace

RealBTarget::RealBTarget(BTarget *owner_, const unsigned short round_) : owner(owner_), round(round_)
{
    registerRealBTarget(this, round_);
}

RealBTarget::RealBTarget(BTarget *owner_, const unsigned short round_, const bool add) : owner(owner_), round(round_)
{
    if (add)
    {
        registerRealBTarget(this, round_);
    }
}

bool RealBTarget::checkDepsChanged() const
{
    const string_view cachedDependencies = bTargetCaches[getBTarget()->cacheIndex].depsCache;
    if (cachedDependencies.size() < sizeof(uint32_t))
    {
        printErrorMessage(FORMAT("Build cache dependency list is truncated.\nTarget: {}\nEntry size: {} bytes\n"
                                 "Minimum size: {} bytes",
                                 getBTarget()->getPrintName(), cachedDependencies.size(), sizeof(uint32_t)));
    }

    const char *ptr = cachedDependencies.data();
    uint64_t bytesRead = 0;
    const uint32_t cachedCount = readUint32(ptr, bytesRead);
    const uint64_t expectedSize = sizeof(uint32_t) + static_cast<uint64_t>(cachedCount) * sizeof(uint32_t);
    if (cachedDependencies.size() != expectedSize)
    {
        printErrorMessage(FORMAT("Build cache dependency list has an invalid size.\nTarget: {}\n"
                                 "Dependency count: {}\nExpected size: {} bytes\nActual size: {} bytes",
                                 getBTarget()->getPrintName(), cachedCount, expectedSize, cachedDependencies.size()));
    }

    if (cachedCount != dependenciesSize)
    {
        return true;
    }

    for (uint32_t i = 0; i < cachedCount; ++i)
    {
        const uint32_t cacheIndex = readUint32(ptr, bytesRead);
        if (cacheIndex >= bTargetCaches.size())
        {
            printErrorMessage(FORMAT("Build cache dependency index is out of range.\nTarget: {}\n"
                                     "Dependency position: {}\nCache index: {}\nCache entry count: {}",
                                     getBTarget()->getPrintName(), i, cacheIndex, bTargetCaches.size()));
        }
        BTarget *bt = bTargetCaches[cacheIndex].bTarget;
        if (!bt)
        {
            return true;
        }
        const auto dependency = dependencies.find(&bt->realBTargets[0]);
        if (dependency == dependencies.end() || !isBlockingRelation(dependency->getRelationType()))
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
        if (isBlockingRelation(rbt.getRelationType()))
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

void BTarget::initializeBTarget(bool makeDirectory)
{
    id = total;
    ++total;
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (makeDirectory)
        {
            string directory(configureNode->filePath);
            directory += slashc;
            directory += name;
            std::filesystem::create_directory(directory);
        }
    }

    const auto it = nameToIndexMap.find(cacheName);
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (it == nameToIndexMap.end())
        {
            cacheIndex = bTargetCaches.size();
            bTargetCaches.emplace_back().name = cacheName;
            if (!nameToIndexMap.emplace(cacheName, cacheIndex).second)
            {
                printErrorMessage(FORMAT("Could not register a new target cache key.\nTarget: {}\nCache key: {}",
                                         getPrintName(), cacheName));
            }
            newlyAdded = true;
        }
        else
        {
            cacheIndex = it->second;
        }
    }
    else
    {
        if (it == nameToIndexMap.end())
        {
            printErrorMessage(FORMAT("Target is missing from the configuration cache.\nTarget: {}\nCache key: {}\n"
                                     "Hint: run hbuild --reconfigure to regenerate the project cache.",
                                     name, cacheName));
        }
        cacheIndex = it->second;

        if (launchesProcess)
        {
            RealBTarget &rb = realBTargets[0];
            const char *ptr = bTargetCaches[cacheIndex].getBuildFooter().data();
            uint64_t bytesRead = 8;
            rb.completionTime = readUint64(ptr, bytesRead);
        }
    }
    BTarget *&cacheTarget = bTargetCaches[cacheIndex].bTarget;
    if (cacheTarget != nullptr)
    {
        printErrorMessage(FORMAT("Two targets use the same cache key.\nExisting target: {}\nNew target: {}\n"
                                 "Cache key: {}",
                                 cacheTarget->getPrintName(), getPrintName(), cacheName));
    }
    cacheTarget = this;
}

BTarget::BTarget(string name_, const bool launchesProcess_, const BTargetType type_)
    : name(lowerCase(std::move(name_))), cacheName(rapidhash(name.data(), name.size())), bTargetType(type_),
      launchesProcess(launchesProcess_), realBTargets{RealBTarget(this, 0), RealBTarget(this, 1)}
{
    initializeBTarget(false);
}

BTarget::BTarget(string name_, const bool launchesProcess_, const BTargetType type_, const bool buildExplicit_,
                 bool makeDirectory)
    : name(lowerCase(std::move(name_))), cacheName(rapidhash(name.data(), name.size())), bTargetType(type_),
      launchesProcess(launchesProcess_), buildExplicit(buildExplicit_),
      realBTargets{RealBTarget(this, 0), RealBTarget(this, 1)}
{
    initializeBTarget(makeDirectory);
}

BTarget::BTarget(string name_, const bool launchesProcess_, const BTargetType type_, const bool buildExplicit_,
                 bool makeDirectory, const bool add0, const bool add1)
    : name(lowerCase(std::move(name_))), cacheName(rapidhash(name.data(), name.size())), bTargetType(type_),
      launchesProcess(launchesProcess_), buildExplicit(buildExplicit_),
      realBTargets{RealBTarget(this, 0, add0), RealBTarget(this, 1, add1)}
{
    initializeBTarget(makeDirectory);
}

BTarget::BTarget(string name_, const uint64_t cacheName_, const bool launchesProcess_, const BTargetType type_)
    : name(lowerCase(std::move(name_))), cacheName(cacheName_), bTargetType(type_), launchesProcess(launchesProcess_),
      realBTargets{RealBTarget(this, 0), RealBTarget(this, 1)}
{
    initializeBTarget(false);
}

BTarget::BTarget(string name_, const uint64_t cacheName_, const bool launchesProcess_, const BTargetType type_,
                 const bool buildExplicit_, bool makeDirectory)
    : name(lowerCase(std::move(name_))), cacheName(cacheName_), bTargetType(type_), launchesProcess(launchesProcess_),
      buildExplicit(buildExplicit_), realBTargets{RealBTarget(this, 0), RealBTarget(this, 1)}
{
    initializeBTarget(makeDirectory);
}

BTarget::BTarget(string name_, const uint64_t cacheName_, const bool launchesProcess_, const BTargetType type_,
                 const bool buildExplicit_, bool makeDirectory, const bool add0, const bool add1)
    : name(lowerCase(std::move(name_))), cacheName(cacheName_), bTargetType(type_), launchesProcess(launchesProcess_),
      buildExplicit(buildExplicit_), realBTargets{RealBTarget(this, 0, add0), RealBTarget(this, 1, add1)}
{
    initializeBTarget(makeDirectory);
}

BTarget::~BTarget()
{
}

void BTarget::writeBuildCacheFooterAtBuildTime(string &buffer) const
{
    writeUint64(buffer, realBTargets[0].cumulativeHash);
    writeUint64(buffer, realBTargets[0].completionTime);
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
    // Non-process aggregate targets still need to discard a provisional reason before their status is propagated.
    refreshUpdateStatus();
    return false;
}

bool BTarget::isEventCompleted(Builder &builder, string_view message)
{
    return false;
}

bool BTarget::refreshUpdateStatus()
{
    RealBTarget &rb = realBTargets[0];

    // If we previously said UPDATE_NEEDED because of reasonForUpdate, but that
    // reason no longer needs an update, invalidate our status so it gets rechecked.
    const bool invalidated = rb.updateStatus == UpdateStatus::UPDATE_NEEDED && rb.reasonForUpdate &&
                             rb.reasonForUpdate->realBTargets[0].updateStatus == UpdateStatus::UPDATE_NOT_NEEDED;
    if (invalidated)
    {
        rb.updateStatus = UpdateStatus::UNCHECKED;
        rb.reasonForUpdate = nullptr;
    }

    if (rb.updateStatus == UpdateStatus::UNCHECKED)
    {
        setUpdateStatus();
    }

    return rb.updateStatus == UpdateStatus::UPDATE_NEEDED;
}

void BTarget::setUpdateStatus()
{
    RealBTarget &rb = realBTargets[0];
    if (rb.updateStatus != UpdateStatus::UNCHECKED)
    {
        return;
    }
    rb.reasonForUpdate = nullptr;

    uint64_t highestTime;
    if (launchesProcess)
    {
        uint64_t bytesRead = 0;
        const char *ptr = bTargetCaches[cacheIndex].getBuildFooter().data();
        if (const uint64_t cumulativeHash = readUint64(ptr, bytesRead); cumulativeHash != rb.cumulativeHash)
        {
            rb.updateStatus = UpdateStatus::UPDATE_NEEDED;
            return;
        }
        highestTime = rb.completionTime;
    }
    else
    {
        // Non-process targets may represent a file directly and seed completionTime before entering this function.
        highestTime = rb.completionTime == -1 ? 0 : rb.completionTime;
    }

    for (const RBTWithType &rbt : rb.dependencies)
    {
        if (isBlockingRelation(rbt.getRelationType()))
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
            rb.reasonForUpdate = depRb->getBTarget();
            return;
        }

        if (depRb->completionTime > highestTime)
        {
            if (launchesProcess)
            {
                rb.updateStatus = UpdateStatus::UPDATE_NEEDED;
                rb.reasonForUpdate = depRb->getBTarget();
                return;
            }
            highestTime = depRb->completionTime;
        }
    }

    rb.updateStatus = UpdateStatus::UPDATE_NOT_NEEDED;

    if (!launchesProcess)
    {
        // Completion time is the newest directly represented file or blocking dependency.
        rb.completionTime = highestTime;
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
            printErrorMessage(FORMAT("Build cache verification failed: footer size mismatch.\nTarget: {}\n"
                                     "Expected size: 16 bytes\nActual size: {} bytes",
                                     getPrintName(), buildCache.size()));
        }
        uint64_t bytesRead = 0;
        verifyBTargetHeader(buildCache, bytesRead);
    }
}

void BTarget::verifyBTargetHeader(string_view buildCache, uint64_t &bytesRead) const
{
    if (newlyAdded)
    {
        bytesRead += 16;
        return;
    }
    if (buildCache.size() < 16)
    {
        printErrorMessage(FORMAT("Build cache verification failed: footer is truncated.\nTarget: {}\n"
                                 "Minimum size: 16 bytes\nActual size: {} bytes",
                                 getPrintName(), buildCache.size()));
    }
    if (const uint64_t commandHash = readUint64(buildCache.data(), bytesRead);
        commandHash != realBTargets[0].cumulativeHash)
    {
        printErrorMessage(FORMAT("Build cache verification failed: command hash mismatch.\nTarget: {}\n"
                                 "Current hash: {}\nCached hash: {}",
                                 getPrintName(), realBTargets[0].cumulativeHash, commandHash));
    }
    if (const uint64_t completionTime = readUint64(buildCache.data(), bytesRead);
        completionTime != realBTargets[0].completionTime)
    {
        printErrorMessage(FORMAT("Build cache verification failed: completion time mismatch.\nTarget: {}\n"
                                 "Current time: {}\nCached time: {}",
                                 getPrintName(), realBTargets[0].completionTime, completionTime));
    }
}

// selectiveBuild is set for the children if hbuild is executed in parent dir. selectiveBuild is set for all
// targets that are present in cmdTargets. if a target explicitBuild is true, then it must be present in cmdTargets for
// its selectiveBuild to be true.
void BTarget::setSelectiveBuild()
{
    // A cycle-suppressed ObjectFileProducer relation may have selected this target before its own round-one turn.
    // Selection is monotonic for one Builder invocation, so retain that propagated state.
    selectiveBuild = selectiveBuild || cmdTargets.contains(name);
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

bool readBool(const char *ptr, uint64_t &bytesRead)
{
    bool result;
    memcpy(&result, ptr + bytesRead, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

uint8_t readUint8(const char *ptr, uint64_t &bytesRead)
{
    uint8_t result;
    memcpy(&result, ptr + bytesRead, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

uint32_t readUint32(const char *ptr, uint64_t &bytesRead)
{
    uint32_t result;
    memcpy(&result, ptr + bytesRead, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

uint64_t readUint64(const char *ptr, uint64_t &bytesRead)
{
    uint64_t result;
    memcpy(&result, ptr + bytesRead, sizeof(result));
    bytesRead += sizeof(result);
    return result;
}

string_view readStringView(const char *ptr, uint64_t &bytesRead)
{
    const uint32_t strSize = readUint32(ptr, bytesRead);
    const uint64_t offset = bytesRead;
    bytesRead += strSize;
    return {ptr + offset, strSize};
}

Node *readHalfNode(const char *ptr, uint64_t &bytesRead)
{
    const uint32_t nodeIndex = readUint32(ptr, bytesRead);
    return nodeIndices[nodeIndex];
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
