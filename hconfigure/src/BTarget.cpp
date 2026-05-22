#include "BTarget.hpp"
#include "BuildSystemFunctions.hpp"
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

    const uint32_t n = static_cast<uint32_t>(graphEdges.size());

    // Borrow indexInTopologicalSort as a scratch dependents-counter for Kahn's algorithm.
    // Safe: the caller writes the real index from sorted[] after this function returns.
    // The cycle-error path terminates inside printErrorMessage, so no cleanup needed there.
    //
    // Bit layout while this function runs:
    //   bits 0–18 : remaining dependents count  (max 512 K − 1 fits in 19 bits)
    //   bit  19   : DFS visited flag
    //   bit  20   : DFS on-stack flag
    static constexpr uint32_t kCountMask = (1u << 19) - 1;
    static constexpr uint32_t kVisited = 1u << 19;
    static constexpr uint32_t kOnStack = 1u << 20;

    vector<RealBTarget *> noEdges;
    noEdges.reserve(n); // avoid ~log₂(n) reallocations
    uint32_t noEdgesCount = 0;

    sorted.clear();
    sorted.resize(n);
    cycleExists = false;

    for (RealBTarget *r : graphEdges)
    {
        r->indexInTopologicalSort = static_cast<uint32_t>(r->dependents.size());
        if (!r->indexInTopologicalSort)
            noEdges.push_back(r);
    }

    uint32_t index = n - 1;
    while (noEdgesCount != static_cast<uint32_t>(noEdges.size()))
    {
        RealBTarget *rb = noEdges[noEdgesCount++];
        sorted[index--] = rb;
        for (const RBTWithType &rbt : rb->dependencies)
        {
            if (!--rbt.getPointer()->indexInTopologicalSort)
                noEdges.push_back(rbt.getPointer());
        }
    }

    if (noEdgesCount != n) // not all nodes processed → cycle; replaces edgesCount check
    {
        cycleExists = true;

        for (RealBTarget *r : graphEdges)
        {
            if (r->indexInTopologicalSort & kCountMask)
                cycle.emplace_back(r);
        }

        string errorString;
        vector<RealBTarget *> currentPath;
        currentPath.reserve(cycle.size());

        for (RealBTarget *node : cycle)
        {
            if (!(node->indexInTopologicalSort & kVisited))
            {
                if (findCycleDFS(node, currentPath, errorString))
                    break;
            }
        }

        printErrorMessage(errorString);
    }
}

bool RealBTarget::findCycleDFS(RealBTarget *node, vector<RealBTarget *> &currentPath, string &errorString)
{
    static constexpr uint32_t kCountMask = (1u << 19) - 1;
    static constexpr uint32_t kVisited = 1u << 19;
    static constexpr uint32_t kOnStack = 1u << 20;

    node->indexInTopologicalSort |= kVisited | kOnStack;
    currentPath.push_back(node);

    for (const RBTWithType &rbt : node->dependencies)
    {
        RealBTarget *dep = rbt.getPointer();
        if (dep->indexInTopologicalSort & kCountMask) // cycle participant
        {
            if (dep->indexInTopologicalSort & kOnStack)
            {
                if (auto cycleStart = std::find(currentPath.begin(), currentPath.end(), dep);
                    cycleStart != currentPath.end())
                {
                    errorString += "Cycle found: ";
                    for (auto it = cycleStart; it != currentPath.end(); ++it)
                        errorString += (*it)->getBTarget()->getPrintName() + " -> ";
                    errorString += dep->getBTarget()->getPrintName() + "\n";
                    // no cleanup needed: printErrorMessage terminates
                    return true;
                }
            }
            else if (!(dep->indexInTopologicalSort & kVisited))
            {
                if (findCycleDFS(dep, currentPath, errorString))
                    return true;
            }
        }
    }

    currentPath.pop_back();
    node->indexInTopologicalSort &= ~kOnStack;
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
    const char *ptr = fileTargetCaches[getBTarget()->cacheIndex].depsCache.data();
    uint32_t bytesRead = 0;
    const uint32_t cachedCount = readUint32(ptr, bytesRead);

    if (cachedCount != dependenciesSize)
    {
        return true;
    }

    for (uint32_t i = 0; i < cachedCount; ++i)
    {
        BTarget *bt = fileTargetCaches[readUint32(ptr, bytesRead)].bTarget;
        if (!bt || !dependencies.contains(&bt->realBTargets[0]))
        {
            return true;
        }
    }

    return false;
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
            cacheIndex = fileTargetCaches.size();
            fileTargetCaches.emplace_back().name = cacheName;
            newlyAdded = true;
        }
        else
        {
            cacheIndex = it->second;
            /*char *ptr = const_cast<char *>(fileTargetCaches[cacheIndex].depsCache.data());
            // claude is this correct? how do I extract
            if (const bool launchesProcessCached = *ptr; launchesProcessCached != launchesProcess)
            {
                printErrorMessage(FORMAT("Target\nname: {}\ncacheName: {}\n has different current launchesProcess "
                                         "value then the cached one. {} vs {}\n",
                                         cacheName, name, launchesProcess, launchesProcessCached));
            }*/
        }

        checkForSameTargetName(this, cacheName);
    }
    else
    {
        if (it == nameToIndexMap.end())
        {
            printErrorMessage(FORMAT("Target\nname: {}\ncacheName: {}\n not found in config-cache.\nMaybe you need to "
                                     "run hhelper first to update the target-cache.\n",
                                     cacheName, name));
        }
        cacheIndex = it->second;

        if (launchesProcess)
        {
            RealBTarget &rb = realBTargets[0];
            const char *ptr = fileTargetCaches[cacheIndex].getBuildFooter().data();
            uint32_t bytesRead = 8;
            rb.launchTime = readUint64(ptr, bytesRead);
        }
    }
    fileTargetCaches[cacheIndex].bTarget = this;
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

void BTarget::setFileStatus()
{
    RealBTarget &rb = realBTargets[0];
    assert(rb.updateStatus == UpdateStatus::UNCHECKED);

    uint64_t highestTime;
    if (launchesProcess)
    {
        uint32_t bytesRead = 0;
        const char *ptr = fileTargetCaches[cacheIndex].getBuildFooter().data();
        if (const uint64_t compileHash = readUint64(ptr, bytesRead); compileHash != rb.cumulativeHash)
        {
            rb.updateStatus = UpdateStatus::UPDATED_NEEDED;
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
            depRb->getBTarget()->setFileStatus();
        }

        if (depRb->updateStatus == UpdateStatus::UPDATED_NEEDED)
        {
            rb.updateStatus = UpdateStatus::UPDATED_NEEDED;
            return;
        }

        // UPDATE_NOT_NEEDED
        if (depRb->launchTime > highestTime)
        {
            // if we do launch the process
            if (launchesProcess)
            {
                rb.updateStatus = UpdateStatus::UPDATED_NEEDED;
                return;
            }

            // if we don't (the default)
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

void BTarget::cppStandAloneCommand(flat_hash_set<string> &createdDirs, string &scriptContents, const string &scriptDir)
{
}

void BTarget::writeConfigCacheAtConfigTime(string &buffer)
{
    const string_view buildCache = fileTargetCaches[cacheIndex].configCache;
    buffer.append(buildCache.begin(), buildCache.end());
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