/// \file
/// Defines an ISPC object producer associated with one C++ target.

#ifndef HMAKE_ISPCTARGET_HPP
#define HMAKE_ISPCTARGET_HPP

#include "ObjectFileProducer.hpp"

class CppTarget;
class IspcHeader;
class IspcObject;

/// Produces all ISPC objects owned by one `CppTarget`.
///
/// One `IspcHeader` and one `IspcObject` action are created per source. Header actions feed the C++ target's lazy
/// `beforeTarget`; object actions feed this aggregate target. The aggregate registers its objects as an implementation
/// link requirement of the owning C++ target.
class IspcTarget final : public ObjectFileProducer
{
  public:
    CppTarget *cppTarget = nullptr;
    Node *myBuildDir = nullptr;
    vector<Node *> sourceNodes;
    vector<IspcHeader *> headerTargets;
    vector<IspcObject *> objectTargets;
    /// Shared command prefixes prepared once from the associated C++ target's effective compile environment.
    string headerCommand;
    string objectCommand;
    uint64_t headerCommandHash = 0;
    uint64_t objectCommandHash = 0;

    /// Toolchain policy comes from the owning Configuration; build mode restores only source nodes from the cache.
    explicit IspcTarget(CppTarget *cppTarget_);

    static uint64_t getCacheName(const CppTarget *cppTarget);

    IspcTarget &addSource(Node *source);
    void getObjectFiles(std::pmr::vector<Node *> &objectNodes, bool includeRequiredProducers) const override;
    /// Lazily prepares shared command prefixes and hashes. Round zero calls this only after the owning C++ target has
    /// flattened its compile usage requirements.
    void initializeCommands();
    string getPrintName() const override;
    void writeConfigCacheAtConfigTime(string &buffer) override;

  private:
    bool commandsInitialized = false;
    void initializeGraph();
    void initializeSource(Node *source);
    void readConfigCacheAtBuildTime();
    void validate() const;
};

/// Generates `<source>.generated.h` and records the transitive ISPC include list.
class IspcHeader final : public BTarget
{
  public:
    IspcTarget *target = nullptr;
    Node *sourceNode = nullptr;
    Node *finalHeader = nullptr;

    span<const uint32_t> cachedDependencies;
    vector<Node *> discoveredDependencies;
    bool dependenciesRefreshed = false;

    IspcHeader(IspcTarget *target_, Node *sourceNode_);

    void getCommand(std::pmr::string &command) const;
    void setUpdateStatus() override;
    bool isEventRegistered(Builder &builder) override;
    bool isEventCompleted(Builder &builder, string_view message) override;
    string getPrintName() const override;
    void writeBuildCacheAtConfigTime(string &buffer) override;
    void writeBuildCacheAtBuildTime(string &buffer) override;
    void verifyBuildCache(string_view buildCache) const override;

  private:
    /// Hashes dependency identities and contents. A nonzero `modifiedAfter` turns dependencies changed while an
    /// action was running into zero hashes, forcing another build pass.
    uint64_t getDependencyHash(uint64_t modifiedAfter = 0) const;
    void parseDependencyList();
};

/// Compiles one `.ispc` source. A multi-target invocation emits a common object plus one object per ISA.
class IspcObject final : public ObjectFile
{
  public:
    IspcTarget *target = nullptr;
    /// Retained so an unchanged generated header can cut off C++ recompilation without suppressing an ISPC object
    /// rebuild after the header action refreshed its dependency list.
    IspcHeader *headerTarget = nullptr;
    Node *sourceNode = nullptr;

    IspcObject(IspcTarget *target_, IspcHeader *headerTarget_, Node *sourceNode_);

    void getCommand(std::pmr::string &command) const;
    void setUpdateStatus() override;
    bool isEventRegistered(Builder &builder) override;
    bool isEventCompleted(Builder &builder, string_view message) override;
    string getPrintName() const override;
    void writeBuildCacheAtBuildTime(string &buffer) override;
};

#endif // HMAKE_ISPCTARGET_HPP
