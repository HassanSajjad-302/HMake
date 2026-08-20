#include "ObjectFileProducer.hpp"

#include <algorithm>
#include <utility>

namespace
{
constexpr uint32_t dependencyTypeBits = 3;
constexpr uint32_t opDependencyMask = 1;
constexpr uint32_t linkDependencyMask = 2;
constexpr uint32_t acyclicDependencyMask = 4;

uint32_t packDependency(const ObjectFileProducer *producer, const OpDepInfo dependency)
{
    return producer->cacheIndex << dependencyTypeBits | (dependency.isOpDependency() ? opDependencyMask : 0) |
           (dependency.isLinkDependency() ? linkDependencyMask : 0) |
           (dependency.isAcyclicDependency() ? acyclicDependencyMask : 0);
}

} // namespace

ObjectFileProducer::ObjectFileProducer(string name_, const BTargetType bTargetType, const bool buildExplicit,
                                       const bool makeDirectory)
    : BTarget(std::move(name_), false, bTargetType, buildExplicit, makeDirectory)
{
    readObjectFileProducerConfigCache();
}

ObjectFileProducer::ObjectFileProducer(string name_, const uint64_t cacheName_, const BTargetType bTargetType,
                                       const bool buildExplicit, const bool makeDirectory)
    : BTarget(std::move(name_), cacheName_, false, bTargetType, buildExplicit, makeDirectory)
{
    readObjectFileProducerConfigCache();
}

void ObjectFileProducer::populateReqAndUseReqObjectFileProducers()
{
    const auto populate = [this](OpDepInfoMap &dependencies, PloatDepInfoMap &ploatDependencies) {
        // Direct declarations are available before round one. Follow those declarations to a fixed point instead of
        // relying on dependency completion order: UE deliberately suppresses scheduler edges for semantic cycles.
        STACK_PMR_VECTOR(ObjectFileProducer *, pending, dependencies.size() + 1)
        for (const auto &entry : dependencies)
        {
            if (entry.first != this)
            {
                pending.emplace_back(entry.first);
            }
        }

        for (size_t position = 0; position < pending.size(); ++position)
        {
            ObjectFileProducer *producer = pending[position];
            const OpDepInfo dependency = dependencies.find(producer)->second;

            for (const auto &[exportedProducer, exported] : producer->useReqObjectFileProducers)
            {
                // Returning to the root contributes no new dependency. Its outgoing declarations were seeded above,
                // and omitting the self-entry also prevents aliasing this map while iterating an exported map.
                if (exportedProducer == this)
                {
                    continue;
                }

                const OpDepInfo propagated = dependency.intersect(exported);
                if ((propagated.isOpDependency() || propagated.isLinkDependency()) &&
                    mergeDependency(dependencies, exportedProducer, propagated))
                {
                    // A producer is revisited only when one of its three monotonic facets changes, so cycles converge
                    // after a small bounded number of iterations.
                    pending.emplace_back(exportedProducer);
                }
            }

            if (dependency.isLinkDependency())
            {
                for (const auto &[ploat, ploatDependency] : producer->useReqPloatDeps)
                {
                    mergePloatDependency(
                        ploatDependencies, ploat,
                        PloatDepInfo{dependency.isAcyclicDependency() && ploatDependency.isAcyclicDependency()});
                }
            }
        }
    };

    populate(reqObjectFileProducers, reqPloatDeps);
    populate(useReqObjectFileProducers, useReqPloatDeps);
}

void ObjectFileProducer::completeRoundOne()
{
    hasObjectFiles |= !prebuiltObjects.empty();

    if constexpr (bsMode == BSMode::BUILD)
    {
        for (Node *object : prebuiltObjects)
        {
            object->doStatFile = true;
        }

        // Round one runs after the complete target graph has been reconstructed. Add nonblocking compile-usage
        // propagation now, but only when at least one path to the producer is known to be acyclic.
        FOR_REQ_OBJECT_FILE_PRODUCERS(this, producer, dependency)
        {
            if (dependency.isOpDependency() && dependency.isAcyclicDependency())
            {
                realBTargets[0].addDep<BTargetType::UNKNOWN, RelationType::SELECTIVE>(&producer->realBTargets[0]);
            }
        }
    }
    else
    {
        populateReqAndUseReqObjectFileProducers();
    }
}

void ObjectFileProducer::setUpdateStatus()
{
    BTarget::setUpdateStatus();
    RealBTarget &rb = realBTargets[0];
    if (rb.updateStatus == UpdateStatus::UPDATE_NOT_NEEDED)
    {
        for (const Node *object : prebuiltObjects)
        {
            rb.completionTime = std::max(rb.completionTime, object->lastWriteTime);
        }
    }
}

void ObjectFileProducer::getObjectFiles(std::pmr::vector<Node *> &objectNodes,
                                        const bool includeRequiredProducers) const
{
    objectNodes.insert(objectNodes.end(), prebuiltObjects.begin(), prebuiltObjects.end());
    if (!includeRequiredProducers)
    {
        return;
    }

    FOR_REQ_OBJECT_FILE_PRODUCERS(this, producer, dependency)
    {
        if (dependency.isLinkDependency())
        {
            producer->getObjectFiles(objectNodes, false);
        }
    }
}

void ObjectFileProducer::writeConfigCacheAtConfigTime(string &buffer)
{
    STACK_PMR_VECTOR(ObjectFileProducer *, sortedReqObjectFileProducers, reqObjectFileProducers.size())
    for (const auto &entry : reqObjectFileProducers)
    {
        sortedReqObjectFileProducers.emplace_back(entry.first);
    }
    std::ranges::sort(sortedReqObjectFileProducers, std::ranges::less{}, &BTarget::cacheIndex);

    writeUint32(buffer, sortedReqObjectFileProducers.size());
    for (ObjectFileProducer *producer : sortedReqObjectFileProducers)
    {
        writeUint32(buffer, packDependency(producer, reqObjectFileProducers.find(producer)->second));
    }

    writeUint32(buffer, prebuiltObjects.size());
    for (const Node *object : prebuiltObjects)
    {
        writeNode(buffer, object);
    }
}

void ObjectFileProducer::verifyConfigCache(const string_view configCache) const
{
    uint32_t bytesRead = 0;
    verifyObjectFileProducerConfigCache(configCache, bytesRead);
}

void ObjectFileProducer::verifyObjectFileProducerConfigCache(const string_view configCache, uint32_t &bytesRead) const
{
    const uint32_t cachedDependencyCount = readUint32(configCache.data(), bytesRead);
    if (cachedDependencyCount != reqObjectFileProducers.size())
    {
        printErrorMessage(FORMAT("Configuration cache verification failed: object-producer dependency count "
                                 "mismatch.\nProducer: {}\nCurrent count: {}\nCached count: {}",
                                 getPrintName(), reqObjectFileProducers.size(), cachedDependencyCount));
    }

    flat_hash_set<uint32_t> currentDependencies;
    currentDependencies.reserve(reqObjectFileProducers.size());
    for (const auto &[producer, dependency] : reqObjectFileProducers)
    {
        currentDependencies.emplace(packDependency(producer, dependency));
    }
    for (uint32_t position = 0; position < cachedDependencyCount; ++position)
    {
        const uint32_t cachedDependency = readUint32(configCache.data(), bytesRead);
        if (!currentDependencies.contains(cachedDependency))
        {
            printErrorMessage(FORMAT("Configuration cache verification failed: object-producer dependency "
                                     "is missing or has different facets.\nProducer: {}\nDependency position: {}\n"
                                     "Cached value: {}",
                                     getPrintName(), position, cachedDependency));
        }
    }

    const uint32_t cachedPrebuiltCount = readUint32(configCache.data(), bytesRead);
    if (cachedPrebuiltCount != prebuiltObjects.size())
    {
        printErrorMessage(FORMAT("Configuration cache verification failed: prebuilt-object count mismatch.\n"
                                 "Producer: {}\nCurrent count: {}\nCached count: {}",
                                 getPrintName(), prebuiltObjects.size(), cachedPrebuiltCount));
    }
    for (uint32_t position = 0; position < cachedPrebuiltCount; ++position)
    {
        const Node *cachedObject = readHalfNode(configCache.data(), bytesRead);
        if (position < prebuiltObjects.size() && cachedObject != prebuiltObjects[position])
        {
            printErrorMessage(FORMAT("Configuration cache verification failed: prebuilt-object mismatch.\n"
                                     "Producer: {}\nPosition: {}\nCurrent object: {}\nCached object: {}",
                                     getPrintName(), position, prebuiltObjects[position]->filePath,
                                     cachedObject->filePath));
        }
    }
}

void ObjectFileProducer::readObjectFileProducerConfigCache()
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        const string_view configCache = bTargetCaches[cacheIndex].configCache;
        const char *ptr = configCache.data();
        const uint32_t dependencyCount = readUint32(ptr, configCacheRead);
        cachedReqObjectFileProducers = span{reinterpret_cast<const uint32_t *>(ptr + configCacheRead), dependencyCount};
        configCacheRead += dependencyCount * sizeof(uint32_t);

        const uint32_t prebuiltCount = readUint32(ptr, configCacheRead);
        prebuiltObjects.reserve(prebuiltCount);
        for (uint32_t index = 0; index < prebuiltCount; ++index)
        {
            prebuiltObjects.emplace_back(readHalfNode(ptr, configCacheRead));
        }
    }
}
