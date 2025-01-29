#ifndef HMAKE_PREBUILTBASIC_HPP
#define HMAKE_PREBUILTBASIC_HPP

#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
import "Features.hpp";
import "FeaturesConvenienceFunctions.hpp";
import "TargetCache.hpp";
import "btree.h";
#else
#include "BTarget.hpp"
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

    string requirementPreLF;
    string usageRequirementPreLF;

    string requirementPostLF;
    string usageRequirementPostLF;

    string requirementRpathLink;
    string usageRequirementRpathLink;

    string requirementRpath;
    string usageRequirementRpath;

    vector<LibDirNode> usageRequirementLibraryDirectories;
    bool defaultRpath = true;
    bool defaultRpathLink = true;
};

class PrebuiltBasic : public BTarget, public PrebuiltBasicFeatures, public TargetCache
{
  public:
    string outputName;
    vector<const class ObjectFile *> objectFiles;

    node_hash_map<PrebuiltBasic *, PrebuiltDep> requirementDeps;
    node_hash_map<PrebuiltBasic *, PrebuiltDep> usageRequirementDeps;

    btree_map<PrebuiltBasic *, const PrebuiltDep *, IndexInTopologicalSortComparatorRoundZero>
        sortedPrebuiltDependencies;

    flat_hash_set<class ObjectFileProducer *> objectFileProducers;

    vector<LibDirNode> usageRequirementLibraryDirectories;

    TargetType linkTargetType = TargetType::LIBRARY_OBJECT;

    template <typename... U> PrebuiltBasic &PUBLIC_DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, U... deps);
    template <typename... U> PrebuiltBasic &PRIVATE_DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, U... deps);
    template <typename... U> PrebuiltBasic &INTERFACE_DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, U... deps);

    template <typename... U> PrebuiltBasic &DEPS(PrebuiltBasic *dep, Dependency dependency, U... deps);

    template <typename... U>
    PrebuiltBasic &PUBLIC_DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, PrebuiltDep prebuiltDep, U... deps);
    template <typename... U>
    PrebuiltBasic &PRIVATE_DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, PrebuiltDep prebuiltDep, U... deps);
    template <typename... U>
    PrebuiltBasic &INTERFACE_DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, PrebuiltDep prebuiltDep, U... deps);

    template <typename... U>
    PrebuiltBasic &DEPS(PrebuiltBasic *prebuiltTarget, Dependency dependency, PrebuiltDep prebuiltDep, U... deps);

    void populateRequirementAndUsageRequirementDeps();

    PrebuiltBasic(const string &outputName_, TargetType linkTargetType_);
    PrebuiltBasic(const string &outputName_, TargetType linkTargetType_, const string &name_, bool buildExplicit,
                  bool makeDirectory);

    void updateBTarget(Builder &builder, unsigned short round) override;

    void writeTargetConfigCacheAtConfigureTime();
    void readConfigCacheAtBuildTime();

    void addRequirementDepsToBTargetDependencies();

    template <typename T, typename... Property> PrebuiltBasic &assign(T property, Property... properties);
    template <typename T> bool evaluate(T property) const;
};
bool operator<(const PrebuiltBasic &lhs, const PrebuiltBasic &rhs);
void to_json(Json &json, const PrebuiltBasic &prebuiltBasic);

template <typename... U>
PrebuiltBasic &PrebuiltBasic::INTERFACE_DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, U... deps)
{
    usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
    realBTargets[2].addDependency(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(deps))
    {
        return INTERFACE_DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltBasic &PrebuiltBasic::PRIVATE_DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, U... deps)
{
    requirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
    realBTargets[2].addDependency(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(deps))
    {
        return PRIVATE_DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltBasic &PrebuiltBasic::PUBLIC_DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, U... deps)
{
    requirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
    usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
    realBTargets[2].addDependency(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(deps))
    {
        return PUBLIC_DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltBasic &PrebuiltBasic::DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, Dependency dependency, U... deps)
{
    if (dependency == Dependency::PUBLIC)
    {
        requirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
        usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
        realBTargets[2].addDependency(*prebuiltLinkOrArchiveTarget);
    }
    else if (dependency == Dependency::PRIVATE)
    {
        requirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
        realBTargets[2].addDependency(*prebuiltLinkOrArchiveTarget);
    }
    else
    {
        usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
        realBTargets[2].addDependency(*prebuiltLinkOrArchiveTarget);
    }
    if constexpr (sizeof...(deps))
    {
        return DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltBasic &PrebuiltBasic::INTERFACE_DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, PrebuiltDep prebuiltDep,
                                             U... deps)
{
    usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
    realBTargets[2].addDependency(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(deps))
    {
        return INTERFACE_DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltBasic &PrebuiltBasic::PRIVATE_DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, PrebuiltDep prebuiltDep,
                                           U... deps)
{
    requirementDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
    realBTargets[2].addDependency(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(deps))
    {
        return PRIVATE_DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltBasic &PrebuiltBasic::PUBLIC_DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, PrebuiltDep prebuiltDep,
                                          U... deps)
{
    requirementDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
    usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
    realBTargets[2].addDependency(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(deps))
    {
        return PUBLIC_DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltBasic &PrebuiltBasic::DEPS(PrebuiltBasic *prebuiltTarget, Dependency dependency, PrebuiltDep prebuiltDep,
                                   U... deps)
{
    if (dependency == Dependency::PUBLIC)
    {
        requirementDeps.emplace(prebuiltTarget, prebuiltDep);
        usageRequirementDeps.emplace(prebuiltTarget, prebuiltDep);
        realBTargets[2].addDependency(*prebuiltTarget);
    }
    else if (dependency == Dependency::PRIVATE)
    {
        requirementDeps.emplace(prebuiltTarget, prebuiltDep);
        realBTargets[2].addDependency(*prebuiltTarget);
    }
    else
    {
        usageRequirementDeps.emplace(prebuiltTarget, prebuiltDep);
        realBTargets[2].addDependency(*prebuiltTarget);
    }
    if constexpr (sizeof...(deps))
    {
        return DEPS(deps...);
    }
    return *this;
}

template <typename T, typename... Property> PrebuiltBasic &PrebuiltBasic::assign(T property, Property... properties)
{
    if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        linkTargetType = property;
    }
    else if constexpr (std::is_same_v<decltype(property), UseMiniTarget>)
    {
        useMiniTarget = property;
    }
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        return property;
    }
    else
    {
        linkTargetType = property; // Just to fail the compilation. Ensures that all properties are handled.
    }

    if constexpr (sizeof...(properties))
    {
        return assign(properties...);
    }
    else
    {
        return *this;
    }
}

template <typename T> bool PrebuiltBasic::evaluate(T property) const
{
    if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        return linkTargetType == property;
    }
    else if constexpr (std::is_same_v<decltype(property), UseMiniTarget>)
    {
        return useMiniTarget == property;
    }
    else
    {
        linkTargetType = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
}

#endif // HMAKE_PREBUILTBASIC_HPP
