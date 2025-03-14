
#ifndef HMAKE_PREBUILTLINKORARCHIVETARGET_HPP
#define HMAKE_PREBUILTLINKORARCHIVETARGET_HPP

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

    vector<LibDirNode> useReqLibraryDirectories;
    bool defaultRpath = true;
    bool defaultRpathLink = true;
};

class PrebuiltLinkOrArchiveTarget : public BTarget, public TargetCache
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

    PrebuiltLinkOrArchiveTarget(Configuration &config_, const string &outputName_, string directory,
                                TargetType linkTargetType_);
    PrebuiltLinkOrArchiveTarget(Configuration &config_, const string &outputName_, string directory,
                                TargetType linkTargetType_, string name_, bool buildExplicit, bool makeDirectory);

    template <typename T> bool evaluate(T property) const;
    void updateBTarget(Builder &builder, unsigned short round) override;

  private:
    void writeTargetConfigCacheAtConfigureTime();
    void readConfigCacheAtBuildTime();

  public:
    node_hash_map<PrebuiltLinkOrArchiveTarget *, PrebuiltDep> reqDeps;
    node_hash_map<PrebuiltLinkOrArchiveTarget *, PrebuiltDep> useReqDeps;

    btree_map<PrebuiltLinkOrArchiveTarget *, const PrebuiltDep *, IndexInTopologicalSortComparatorRoundZero>
        sortedPrebuiltDependencies;

    flat_hash_set<class ObjectFileProducer *> objectFileProducers;

    vector<LibDirNode> reqLibraryDirectories;
    vector<LibDirNode> useReqLibraryDirectories;

    TargetType linkTargetType = TargetType::LIBRARY_STATIC;

    template <typename... U>
    PrebuiltLinkOrArchiveTarget &publicDeps(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget,
                                            U... prebuiltLinkOrArchiveTargets);
    template <typename... U>
    PrebuiltLinkOrArchiveTarget &privateDeps(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget,
                                             U... prebuiltLinkOrArchiveTargets);
    template <typename... U>
    PrebuiltLinkOrArchiveTarget &interfaceDeps(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget,
                                               U... prebuiltLinkOrArchiveTargets);

    template <typename... U>
    PrebuiltLinkOrArchiveTarget &deps(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, Dependency dependency,
                                      U... prebuiltLinkOrArchiveTargets);

    template <typename... U>
    PrebuiltLinkOrArchiveTarget &publicDeps(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget,
                                            PrebuiltDep prebuiltDep, U... prebuiltLinkOrArchiveTargets);
    template <typename... U>
    PrebuiltLinkOrArchiveTarget &privateDeps(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget,
                                             PrebuiltDep prebuiltDep, U... prebuiltLinkOrArchiveTargets);
    template <typename... U>
    PrebuiltLinkOrArchiveTarget &interfaceDeps(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget,
                                               PrebuiltDep prebuiltDep, U... prebuiltLinkOrArchiveTargets);

    template <typename... U>
    PrebuiltLinkOrArchiveTarget &deps(PrebuiltLinkOrArchiveTarget *prebuiltTarget, Dependency dependency,
                                      PrebuiltDep prebuiltDep, U... prebuiltLinkOrArchiveTargets);

    void populateReqAndUseReqDeps();
    void addReqDepsToBTargetDependencies();
};

template <typename T> bool PrebuiltLinkOrArchiveTarget::evaluate(T property) const
{
    if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        return linkTargetType == property;
    }
    else if constexpr (std::is_same_v<decltype(property), CopyDLLToExeDirOnNTOs>)
    {
        return config.prebuiltLinkOrArchiveTargetFeatures.evaluate(property);
    }
    else
    {
        static_assert(false);
    }
}

bool operator<(const PrebuiltLinkOrArchiveTarget &lhs, const PrebuiltLinkOrArchiveTarget &rhs);
void to_json(Json &json, const PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget);

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::interfaceDeps(
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, U... prebuiltLinkOrArchiveTargets)
{
    useReqDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
    addDependency<2>(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(prebuiltLinkOrArchiveTargets))
    {
        return interfaceDeps(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::privateDeps(
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, U... prebuiltLinkOrArchiveTargets)
{
    reqDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
    addDependency<2>(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(prebuiltLinkOrArchiveTargets))
    {
        return privateDeps(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::publicDeps(
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, U... prebuiltLinkOrArchiveTargets)
{
    reqDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
    useReqDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
    addDependency<2>(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(prebuiltLinkOrArchiveTargets))
    {
        return publicDeps(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::deps(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget,
                                                               const Dependency dependency,
                                                               U... prebuiltLinkOrArchiveTargets)
{
    if (dependency == Dependency::PUBLIC)
    {
        reqDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
        useReqDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
        addDependency<2>(*prebuiltLinkOrArchiveTarget);
    }
    else if (dependency == Dependency::PRIVATE)
    {
        reqDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
        addDependency<2>(*prebuiltLinkOrArchiveTarget);
    }
    else
    {
        useReqDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
        addDependency<2>(*prebuiltLinkOrArchiveTarget);
    }
    if constexpr (sizeof...(prebuiltLinkOrArchiveTargets))
    {
        return deps(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::interfaceDeps(
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, PrebuiltDep prebuiltDep,
    U... prebuiltLinkOrArchiveTargets)
{
    useReqDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
    if constexpr (sizeof...(prebuiltLinkOrArchiveTargets))
    {
        return interfaceDeps(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::privateDeps(
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, PrebuiltDep prebuiltDep,
    U... prebuiltLinkOrArchiveTargets)
{
    reqDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
    addDependency<2>(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(prebuiltLinkOrArchiveTargets))
    {
        return privateDeps(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::publicDeps(
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, PrebuiltDep prebuiltDep,
    U... prebuiltLinkOrArchiveTargets)
{
    reqDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
    useReqDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
    addDependency<2>(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(prebuiltLinkOrArchiveTargets))
    {
        return publicDeps(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::deps(PrebuiltLinkOrArchiveTarget *prebuiltTarget,
                                                               Dependency dependency, PrebuiltDep prebuiltDep,
                                                               U... prebuiltLinkOrArchiveTargets)
{
    if (dependency == Dependency::PUBLIC)
    {
        reqDeps.emplace(prebuiltTarget, prebuiltDep);
        useReqDeps.emplace(prebuiltTarget, prebuiltDep);
        addDependency<2>(*prebuiltTarget);
    }
    else if (dependency == Dependency::PRIVATE)
    {
        reqDeps.emplace(prebuiltTarget, prebuiltDep);
        addDependency<2>(*prebuiltTarget);
    }
    else
    {
        useReqDeps.emplace(prebuiltTarget, prebuiltDep);
    }
    if constexpr (sizeof...(prebuiltLinkOrArchiveTargets))
    {
        return deps(deps...);
    }
    return *this;
}

#endif // HMAKE_PREBUILTLINKORARCHIVETARGET_HPP
