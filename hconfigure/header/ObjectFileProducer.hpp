#ifndef HMAKE_OBJECTFILEPRODUCER_HPP
#define HMAKE_OBJECTFILEPRODUCER_HPP

#include "BuildSystemFunctions.hpp"
#include "ObjectFile.hpp"

#include <ranges>
#include <utility>

class ObjectFileProducer;
class PLOAT;

/// Scheduler metadata carried by a semantic PLOAT dependency.
class PloatDepInfo
{
    // TODO(UE cycles): Remove this facet when UE circular module dependencies no longer require cycle suppression.
    bool acyclicDependency : 1 = true;

  public:
    PloatDepInfo() = default;
    explicit PloatDepInfo(const bool acyclicDependency_) : acyclicDependency(acyclicDependency_)
    {
    }

    bool isAcyclicDependency() const
    {
        return acyclicDependency;
    }
    static constexpr uint32_t getCacheIndex(const uint32_t packed)
    {
        return packed >> 1;
    }
    static constexpr PloatDepInfo fromCache(const uint32_t packed)
    {
        return PloatDepInfo{bool(packed & 1)};
    }

    /// An edge is schedulable when at least one alternative path to the same PLOAT is acyclic.
    PloatDepInfo unite(const PloatDepInfo other) const
    {
        return PloatDepInfo{acyclicDependency || other.acyclicDependency};
    }

    /// A composed path is schedulable only when every segment in it is acyclic.
    PloatDepInfo intersect(const PloatDepInfo exported) const
    {
        return PloatDepInfo{acyclicDependency && exported.acyclicDependency};
    }
};

using PloatDepInfoMap = flat_hash_map<PLOAT *, PloatDepInfo>;

inline void mergePloatDependency(PloatDepInfoMap &dependencies, PLOAT *dependency, const PloatDepInfo dependencyInfo)
{
    const auto [entry, inserted] = dependencies.try_emplace(dependency, dependencyInfo);
    if (!inserted)
    {
        entry->second = entry->second.unite(dependencyInfo);
    }
}

/// Facets carried by a dependency between two ObjectFileProducer targets.
class OpDepInfo
{
    bool opDependency : 1 = false;
    bool linkDependency : 1 = false;
    // TODO(UE cycles): Remove this facet when UE circular module dependencies no longer require cycle suppression.
    bool acyclicDependency : 1 = true;

  public:
    OpDepInfo() = default;
    OpDepInfo(const bool opDependency_, const bool linkDependency_, const bool acyclicDependency_ = true)
        : opDependency(opDependency_), linkDependency(linkDependency_), acyclicDependency(acyclicDependency_)
    {
    }

    bool isOpDependency() const
    {
        return opDependency;
    }
    bool isLinkDependency() const
    {
        return linkDependency;
    }
    bool isAcyclicDependency() const
    {
        return acyclicDependency;
    }
    static constexpr uint32_t getCacheIndex(const uint32_t packed)
    {
        return packed >> 3;
    }
    static constexpr OpDepInfo fromCache(const uint32_t packed)
    {
        return {bool(packed & 1), bool(packed & 2), bool(packed & 4)};
    }

    /// Combines alternative paths to the same producer. A facet is available when either path provides it.
    OpDepInfo unite(const OpDepInfo &other) const
    {
        return {opDependency || other.opDependency, linkDependency || other.linkDependency,
                acyclicDependency || other.acyclicDependency};
    }

    /// Composes consecutive path segments. A facet survives only when both segments provide it.
    OpDepInfo intersect(const OpDepInfo &exported) const
    {
        return {opDependency && exported.opDependency, linkDependency && exported.linkDependency,
                acyclicDependency && exported.acyclicDependency};
    }

    bool operator==(const OpDepInfo &) const = default;
};

using OpDepInfoMap = flat_hash_map<ObjectFileProducer *, OpDepInfo>;

class ObjectFileProducer : public BTarget
{
  public:
    bool hasObjectFiles = false;

    /// Configure-time required producer relationships. Metadata distinguishes compile usage from linked objects.
    OpDepInfoMap reqObjectFileProducers;
    /// Configure-time producer relationships exported to consumers.
    OpDepInfoMap useReqObjectFileProducers;

    /// Configure-time PLOAT requirements carried by an outputless producer until a physical link boundary consumes
    /// its object files. PRIVATE requirements remain in useReqPloatDeps while there is no boundary to absorb them.
    PloatDepInfoMap reqPloatDeps;
    PloatDepInfoMap useReqPloatDeps;

    /// Existing object files consumed by a later LOAT without a compile action in this graph.
    vector<Node *> prebuiltObjects;

    /// Packed cache-index/facet entries restored in build mode. The low three bits contain the dependency booleans.
    span<const uint32_t> cachedReqObjectFileProducers;

    /// Number of bytes consumed by the ObjectFileProducer prefix in this target's config-cache entry.
    uint32_t configCacheRead = 0;

    ObjectFileProducer(string name_, BTargetType bTargetType, bool buildExplicit, bool makeDirectory);
    ObjectFileProducer(string name_, uint64_t cacheName_, BTargetType bTargetType, bool buildExplicit,
                       bool makeDirectory);

    /// Inserts or facet-merges a dependency. Returns true if the map changed.
    static bool mergeDependency(OpDepInfoMap &dependencies, ObjectFileProducer *producer, const OpDepInfo dependency)
    {
        const auto [existing, inserted] = dependencies.try_emplace(producer, dependency);
        if (inserted)
        {
            return true;
        }

        const OpDepInfo previous = existing->second;
        existing->second = previous.unite(dependency);
        return existing->second != previous;
    }

    /// Expands required and usage producers through exported usage relationships.
    void populateReqAndUseReqObjectFileProducers();
    void completeRoundOne() override;
    void setUpdateStatus() override;

    /// Appends this producer's linker inputs and, when requested, those of its flattened required producers.
    virtual void getObjectFiles(std::pmr::vector<Node *> &objectNodes, bool includeRequiredProducers) const;

    void writeConfigCacheAtConfigTime(string &buffer) override;
    void verifyConfigCache(string_view configCache) const override;

  protected:
    void verifyObjectFileProducerConfigCache(string_view configCache, uint32_t &bytesRead) const;

  private:
    void readObjectFileProducerConfigCache();
};

#ifdef BUILD_MODE
#define FOR_REQ_OBJECT_FILE_PRODUCERS(objectFileProducer_, producer_, depInfo_)                                        \
    for (const auto [producer_, depInfo_] : std::views::transform(                                                     \
             (objectFileProducer_)->cachedReqObjectFileProducers, [](const uint32_t packedDependency) {                \
                 return std::pair{static_cast<ObjectFileProducer *>(                                                   \
                                      bTargetCaches[OpDepInfo::getCacheIndex(packedDependency)].bTarget),              \
                                  OpDepInfo::fromCache(packedDependency)};                                             \
             }))
#else
#define FOR_REQ_OBJECT_FILE_PRODUCERS(objectFileProducer_, producer_, depInfo_)                                        \
    for (const auto &[producer_, depInfo_] : (objectFileProducer_)->reqObjectFileProducers)
#endif

#endif // HMAKE_OBJECTFILEPRODUCER_HPP
