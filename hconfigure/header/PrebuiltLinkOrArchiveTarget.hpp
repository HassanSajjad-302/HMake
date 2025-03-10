
#ifndef HMAKE_PREBUILTLINKORARCHIVETARGET_HPP
#define HMAKE_PREBUILTLINKORARCHIVETARGET_HPP

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
#include <Configuration.hpp>

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

class PrebuiltLinkOrArchiveTarget : public BTarget, public TargetCache
{
#ifndef BUILD_MODE
    string outputName;
    string actualOutputName;
    string outputDirectory;
#endif

  public:
    string usageRequirementLinkerFlags;
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

  protected:
    void writeTargetConfigCacheAtConfigureTime();
    void readConfigCacheAtBuildTime();

  public:
    node_hash_map<PrebuiltLinkOrArchiveTarget *, PrebuiltDep> requirementDeps;
    node_hash_map<PrebuiltLinkOrArchiveTarget *, PrebuiltDep> usageRequirementDeps;

    btree_map<PrebuiltLinkOrArchiveTarget *, const PrebuiltDep *, IndexInTopologicalSortComparatorRoundZero>
        sortedPrebuiltDependencies;

    flat_hash_set<class ObjectFileProducer *> objectFileProducers;

    vector<LibDirNode> requirementLibraryDirectories;
    vector<LibDirNode> usageRequirementLibraryDirectories;

    TargetType linkTargetType = TargetType::LIBRARY_STATIC;

    template <typename... U>
    PrebuiltLinkOrArchiveTarget &PUBLIC_DEPS(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, U... deps);
    template <typename... U>
    PrebuiltLinkOrArchiveTarget &PRIVATE_DEPS(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, U... deps);
    template <typename... U>
    PrebuiltLinkOrArchiveTarget &INTERFACE_DEPS(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, U... deps);

    template <typename... U>
    PrebuiltLinkOrArchiveTarget &DEPS(PrebuiltLinkOrArchiveTarget *dep, Dependency dependency, U... deps);

    template <typename... U>
    PrebuiltLinkOrArchiveTarget &PUBLIC_DEPS(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget,
                                             PrebuiltDep prebuiltDep, U... deps);
    template <typename... U>
    PrebuiltLinkOrArchiveTarget &PRIVATE_DEPS(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget,
                                              PrebuiltDep prebuiltDep, U... deps);
    template <typename... U>
    PrebuiltLinkOrArchiveTarget &INTERFACE_DEPS(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget,
                                                PrebuiltDep prebuiltDep, U... deps);

    template <typename... U>
    PrebuiltLinkOrArchiveTarget &DEPS(PrebuiltLinkOrArchiveTarget *prebuiltTarget, Dependency dependency,
                                      PrebuiltDep prebuiltDep, U... deps);

    void populateRequirementAndUsageRequirementDeps();
    void addRequirementDepsToBTargetDependencies();
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
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::INTERFACE_DEPS(
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, U... deps)
{
    usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
    addDependency<2>(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(deps))
    {
        return INTERFACE_DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::PRIVATE_DEPS(
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, U... deps)
{
    requirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
    addDependency<2>(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(deps))
    {
        return PRIVATE_DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::PUBLIC_DEPS(
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, U... deps)
{
    requirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
    usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
    addDependency<2>(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(deps))
    {
        return PUBLIC_DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::DEPS(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget,
                                                               Dependency dependency, U... deps)
{
    if (dependency == Dependency::PUBLIC)
    {
        requirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
        usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
        addDependency<2>(*prebuiltLinkOrArchiveTarget);
    }
    else if (dependency == Dependency::PRIVATE)
    {
        requirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
        addDependency<2>(*prebuiltLinkOrArchiveTarget);
    }
    else
    {
        usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
        addDependency<2>(*prebuiltLinkOrArchiveTarget);
    }
    if constexpr (sizeof...(deps))
    {
        return DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::INTERFACE_DEPS(
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, PrebuiltDep prebuiltDep, U... deps)
{
    usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
    if constexpr (sizeof...(deps))
    {
        return INTERFACE_DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::PRIVATE_DEPS(
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, PrebuiltDep prebuiltDep, U... deps)
{
    requirementDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
    addDependency<2>(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(deps))
    {
        return PRIVATE_DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::PUBLIC_DEPS(
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, PrebuiltDep prebuiltDep, U... deps)
{
    requirementDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
    usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
    addDependency<2>(*prebuiltLinkOrArchiveTarget);
    if constexpr (sizeof...(deps))
    {
        return PUBLIC_DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::DEPS(PrebuiltLinkOrArchiveTarget *prebuiltTarget,
                                                               Dependency dependency, PrebuiltDep prebuiltDep,
                                                               U... deps)
{
    if (dependency == Dependency::PUBLIC)
    {
        requirementDeps.emplace(prebuiltTarget, prebuiltDep);
        usageRequirementDeps.emplace(prebuiltTarget, prebuiltDep);
        addDependency<2>(*prebuiltTarget);
    }
    else if (dependency == Dependency::PRIVATE)
    {
        requirementDeps.emplace(prebuiltTarget, prebuiltDep);
        addDependency<2>(*prebuiltTarget);
    }
    else
    {
        usageRequirementDeps.emplace(prebuiltTarget, prebuiltDep);
    }
    if constexpr (sizeof...(deps))
    {
        return DEPS(deps...);
    }
    return *this;
}

#endif // HMAKE_PREBUILTLINKORARCHIVETARGET_HPP
