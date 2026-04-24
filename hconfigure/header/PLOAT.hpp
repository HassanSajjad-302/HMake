
#ifndef HMAKE_PLOAT_HPP
#define HMAKE_PLOAT_HPP

#include "BTarget.hpp"
#include "Features.hpp"
#include "FeaturesConvenienceFunctions.hpp"
#include "SpecialNodes.hpp"
#include "TargetCache.hpp"
#include "gtl/include/gtl/btree.hpp"

class Configuration;

using gtl::node_hash_map, gtl::btree_set;

// PrebuiltLinkOrArchiveTarget
class PLOAT : public BTarget, public TargetCache
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
    string configCacheBuffer;
    uint32_t configCacheBytesRead = 0;
    bool hasObjectFiles = true;

    string getOutputName() const;
    string getActualOutputName() const;
    string_view getOutputDirectoryV() const;

    PLOAT(Configuration &config_, const string &outputName_, Node *myBuildDir_, TargetType linkTargetType_);
    PLOAT(Configuration &config_, const string &outputName_, Node *myBuildDir_, TargetType linkTargetType_,
          string name_, bool buildExplicit, bool makeDirectory);

    void initializePLOAT();
    template <typename T> bool evaluate(T property) const;
    void completeRoundOne() override;

  private:
    void writeTargetConfigCacheAtConfigureTime();
    void readCacheAtBuildTime();

  public:
    // Following 2 unused at BSMode::Build
    // we need this to be ordered in setLinkCommand. order is deterministic as insertions are supposed to be always
    // in order
    btree_set<PLOAT *, TPointerLess<PLOAT>> reqDeps;
    flat_hash_set<PLOAT *> useReqDeps;

    /// TargetCache::cacheIndex of our direct and transitive dependency PLOAT. It is cached in config-cache.
    vector<uint32_t> reqDepsVecIndices;

    flat_hash_set<class ObjectFileProducer *> objectFileProducers;

    vector<LibDirNode> reqLibraryDirs;
    vector<LibDirNode> useReqLibraryDirs;

    TargetType linkTargetType = TargetType::LIBRARY_STATIC;

    template <typename... U> PLOAT &publicDeps(PLOAT &ploat, U... ploats);
    template <typename... U> PLOAT &privateDeps(PLOAT &ploat, U... ploats);
    template <typename... U> PLOAT &interfaceDeps(PLOAT &ploat, U... ploats);

    template <typename... U> PLOAT &deps(DepType depType, PLOAT &ploat, U... ploats);

    void populateReqAndUseReqDeps();
    string getPrintName() const override;
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
void to_json(Json &json, const PLOAT &PLOAT);

template <typename... U> PLOAT &PLOAT::interfaceDeps(PLOAT &ploat, U... ploats)
{
    deps(DepType::INTERFACE, ploat);
    if constexpr (sizeof...(ploats))
    {
        return interfaceDeps(ploats...);
    }
    return *this;
}

template <typename... U> PLOAT &PLOAT::privateDeps(PLOAT &ploat, U... ploats)
{
    deps(DepType::PRIVATE, ploat);
    if constexpr (sizeof...(ploats))
    {
        return privateDeps(ploats...);
    }
    return *this;
}

template <typename... U> PLOAT &PLOAT::publicDeps(PLOAT &ploat, U... ploats)
{
    deps(DepType::PUBLIC, ploat);
    if constexpr (sizeof...(ploats))
    {
        return publicDeps(ploats...);
    }
    return *this;
}

template <typename... U> PLOAT &PLOAT::deps(const DepType depType, PLOAT &ploat, U... ploats)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        TargetCache *us = static_cast<TargetCache *>(this);
        TargetCache *ourDep = static_cast<TargetCache *>(&ploat);
        if (ourDep->cacheIndex > us->cacheIndex)
        {
            printErrorMessage(FORMAT("Please declare dependency \n{}\n before its dependent \n{}\nDependency "
                                     "declaration before the dependent is an invariant in HMake.",
                                     fileTargetCaches[ourDep->cacheIndex].name, fileTargetCaches[us->cacheIndex].name));
        }

        if (depType == DepType::PUBLIC)
        {
            reqDeps.emplace(&ploat);
            useReqDeps.emplace(&ploat);
            addDep<1>(ploat);
        }
        else if (depType == DepType::PRIVATE)
        {
            reqDeps.emplace(&ploat);
            addDep<1>(ploat);
        }
        else
        {
            useReqDeps.emplace(&ploat);
            addDep<1>(ploat);
        }
    }
    else
    {
    }

    if constexpr (sizeof...(ploats))
    {
        return deps(depType, ploats...);
    }
    return *this;
}

#endif // HMAKE_PLOAT_HPP
