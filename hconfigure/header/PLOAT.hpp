
#ifndef HMAKE_PLOAT_HPP
#define HMAKE_PLOAT_HPP

#include "BTarget.hpp"
#include "Features.hpp"
#include "ObjectFileProducer.hpp"

class Configuration;

// PrebuiltLinkOrArchiveTarget
class PLOAT : public BTarget
{
#ifndef BUILD_MODE
    string actualOutputName;
    Node *outputDirectory;

  public:
    string outputName;
#endif

  public:
    Configuration &config;
    Node *outputFileNode = nullptr;
    uint64_t configCacheBytesRead = 0;
    bool hasObjectFiles = false;

    string getOutputName() const;
    string getActualOutputName() const;
    string_view getOutputDirectoryV() const;

    PLOAT(Configuration &config_, const string &outputName_, Node *myBuildDir_, TargetType linkTargetType_);
    PLOAT(Configuration &config_, const string &outputName_, Node *myBuildDir_, TargetType linkTargetType_,
          string name_, bool buildExplicit, bool makeDirectory);

    void initializePLOAT();
    template <typename T> bool evaluate(T property) const;
    void setUpdateStatus() override;
    void completeRoundOne() override;

  private:
    void readCacheAtBuildTime();

  public:
    // Configure-time semantic link closures. The facet suppresses only scheduler edges; every entry remains a linker
    // input and is exported according to its visibility.
    PloatDepInfoMap reqDeps;
    PloatDepInfoMap useReqDeps;

    /// Packed TargetCache::cacheIndex and acyclic-path facet for direct and transitive dependency PLOATs.
    vector<uint32_t> cachedReqDeps;

    /// Producers paired directly with this link target by DSC. After their round-one completion, PLOAT inspects each
    /// root's cached semantic closure and creates the required round-zero linker-input dependencies.
    flat_hash_set<class ObjectFileProducer *> rootObjectFileProducers;

    TargetType linkTargetType = TargetType::LIBRARY_STATIC;

    void populateReqAndUseReqDeps();
    string getPrintName() const override;
    void writeConfigCacheAtConfigTime(string &buffer) override;
};

template <typename T> bool PLOAT::evaluate(T property) const
{
    if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        return linkTargetType == property;
    }
    else
    {
        static_assert(false);
    }
}

bool operator<(const PLOAT &lhs, const PLOAT &rhs);

#endif // HMAKE_PLOAT_HPP
