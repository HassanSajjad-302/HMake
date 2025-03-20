
#ifndef HMAKE_PLOAT_HPP
#define HMAKE_PLOAT_HPP

#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
import "Configuration.hpp";
import "Features.hpp";
import "FeaturesConvenienceFunctions.hpp";
import "TargetCache.hpp";
import "btree.h";
#else
#include "BTarget.hpp"
#include "Configuration.hpp"
#include "Features.hpp"
#include "FeaturesConvenienceFunctions.hpp"
#include "TargetCache.hpp"
#include "btree.h"
#endif

using phmap::node_hash_map, phmap::btree_map;

// TODO
//  This is a memory hog.
struct PrebuiltDep
{
    // LF linkerFlags

    string reqPreLF;
    string useReqPreLF;

    string reqPostLF;
    string useReqPostLF;

    string reqRpathLink;
    string useReqRpathLink;

    string reqRpath;
    string useReqRpath;

    vector<LibDirNode> useReqLibraryDirs;
    bool defaultRpath = true;
    bool defaultRpathLink = true;
};

// PrebuiltLinkOrArchiveTarget
class PLOAT : public BTarget, public TargetCache
{
#ifndef BUILD_MODE
    string actualOutputName;
    string outputDirectory;

  public:
    string outputName;
#endif

  public:
    string useReqLinkerFlags;
    Configuration &config;
    Node *outputFileNode = nullptr;

    string getOutputName() const;
    string getActualOutputName() const;
    string_view getOutputDirectoryV() const;

    PLOAT(Configuration &config_, const string &outputName_, string dir, TargetType linkTargetType_);
    PLOAT(Configuration &config_, const string &outputName_, string dir, TargetType linkTargetType_, string name_,
          bool buildExplicit, bool makeDirectory);

    template <typename T> bool evaluate(T property) const;
    void updateBTarget(Builder &builder, unsigned short round) override;

  private:
    void writeTargetConfigCacheAtConfigureTime();
    void readConfigCacheAtBuildTime();

  public:
    node_hash_map<PLOAT *, PrebuiltDep> reqDeps;
    node_hash_map<PLOAT *, PrebuiltDep> useReqDeps;

    btree_map<PLOAT *, const PrebuiltDep *, IndexInTopologicalSortComparatorRoundZero> sortedPrebuiltDependencies;

    flat_hash_set<class ObjectFileProducer *> objectFileProducers;

    vector<LibDirNode> reqLibraryDirs;
    vector<LibDirNode> useReqLibraryDirs;

    TargetType linkTargetType = TargetType::LIBRARY_STATIC;

    template <typename... U> PLOAT &publicDeps(PLOAT &ploat, U... ploats);
    template <typename... U> PLOAT &privateDeps(PLOAT &ploat, U... ploats);
    template <typename... U> PLOAT &interfaceDeps(PLOAT &ploat, U... ploats);

    template <typename... U> PLOAT &deps(DepType depType, PLOAT &ploat, U... ploats);

    template <typename... U> PLOAT &publicDeps(PLOAT &ploat, PrebuiltDep prebuiltDep, U... ploats);
    template <typename... U> PLOAT &privateDeps(PLOAT &ploat, PrebuiltDep prebuiltDep, U... ploats);
    template <typename... U> PLOAT &interfaceDeps(PLOAT &ploat, PrebuiltDep prebuiltDep, U... ploats);

    template <typename... U> PLOAT &deps(DepType depType, PLOAT &ploat, PrebuiltDep prebuiltDep, U... ploats);

    void populateReqAndUseReqDeps();
    void addReqDepsToBTargetDependencies();
};

template <typename T> bool PLOAT::evaluate(T property) const
{
    if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        return linkTargetType == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CopyDLLToExeDirOnNTOs>)
    {
        return config.ploatFeatures.evaluate(property);
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
    useReqDeps.emplace(&ploat, PrebuiltDep{});
    addDependencyNoMutex<2>(ploat);
    if constexpr (sizeof...(ploats))
    {
        return interfaceDeps(deps...);
    }
    return *this;
}

template <typename... U> PLOAT &PLOAT::privateDeps(PLOAT &ploat, U... ploats)
{
    reqDeps.emplace(&ploat, PrebuiltDep{});
    addDependencyNoMutex<2>(ploat);
    if constexpr (sizeof...(ploats))
    {
        return privateDeps(deps...);
    }
    return *this;
}

template <typename... U> PLOAT &PLOAT::publicDeps(PLOAT &ploat, U... ploats)
{
    reqDeps.emplace(&ploat, PrebuiltDep{});
    useReqDeps.emplace(&ploat, PrebuiltDep{});
    addDependencyNoMutex<2>(ploat);
    if constexpr (sizeof...(ploats))
    {
        return publicDeps(deps...);
    }
    return *this;
}

template <typename... U> PLOAT &PLOAT::deps(const DepType depType, PLOAT &ploat, U... ploats)
{
    if (depType == DepType::PUBLIC)
    {
        reqDeps.emplace(&ploat, PrebuiltDep{});
        useReqDeps.emplace(&ploat, PrebuiltDep{});
        addDependencyNoMutex<2>(ploat);
    }
    else if (depType == DepType::PRIVATE)
    {
        reqDeps.emplace(&ploat, PrebuiltDep{});
        addDependencyNoMutex<2>(ploat);
    }
    else
    {
        useReqDeps.emplace(&ploat, PrebuiltDep{});
        addDependencyNoMutex<2>(ploat);
    }
    if constexpr (sizeof...(ploats))
    {
        return deps(depType, ploats...);
    }
    return *this;
}

template <typename... U> PLOAT &PLOAT::interfaceDeps(PLOAT &ploat, PrebuiltDep prebuiltDep, U... ploats)
{
    useReqDeps.emplace(&ploat, prebuiltDep);
    if constexpr (sizeof...(ploats))
    {
        return interfaceDeps(deps...);
    }
    return *this;
}

template <typename... U> PLOAT &PLOAT::privateDeps(PLOAT &ploat, PrebuiltDep prebuiltDep, U... ploats)
{
    reqDeps.emplace(&ploat, prebuiltDep);
    addDependencyNoMutex<2>(ploat);
    if constexpr (sizeof...(ploats))
    {
        return privateDeps(deps...);
    }
    return *this;
}

template <typename... U> PLOAT &PLOAT::publicDeps(PLOAT &ploat, PrebuiltDep prebuiltDep, U... ploats)
{
    reqDeps.emplace(&ploat, prebuiltDep);
    useReqDeps.emplace(&ploat, prebuiltDep);
    addDependencyNoMutex<2>(ploat);
    if constexpr (sizeof...(ploats))
    {
        return publicDeps(deps...);
    }
    return *this;
}

template <typename... U> PLOAT &PLOAT::deps(const DepType depType, PLOAT &ploat, PrebuiltDep prebuiltDep, U... ploats)
{
    if (depType == DepType::PUBLIC)
    {
        reqDeps.emplace(&ploat, prebuiltDep);
        useReqDeps.emplace(&ploat, prebuiltDep);
        addDependencyNoMutex<2>(ploat);
    }
    else if (depType == DepType::PRIVATE)
    {
        reqDeps.emplace(&ploat, prebuiltDep);
        addDependencyNoMutex<2>(ploat);
    }
    else
    {
        useReqDeps.emplace(&ploat, prebuiltDep);
    }
    if constexpr (sizeof...(ploats))
    {
        return deps(depType, ploats...);
    }
    return *this;
}

#endif // HMAKE_PLOAT_HPP
