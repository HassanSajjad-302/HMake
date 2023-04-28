
#ifndef HMAKE_PREBUILTLINKORARCHIVETARGET_HPP
#define HMAKE_PREBUILTLINKORARCHIVETARGET_HPP

#ifdef USE_HEADER_UNITS
import "BasicTargets.hpp";
import "Features.hpp";
import "FeaturesConvenienceFunctions.hpp";
#else
#include "BasicTargets.hpp"
#include "Features.hpp"
#include "FeaturesConvenienceFunctions.hpp"
#endif

class PrebuiltLinkOrArchiveTarget;

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

    bool defaultRpath = true;
    bool defaultRpathLink = true;
};

class PrebuiltLinkOrArchiveTarget : public BTarget, public PrebuiltLinkerFeatures
{
  public:
    string outputDirectory;
    string outputName;
    string actualOutputName;
    string usageRequirementLinkerFlags;
    TargetType linkTargetType;

    map<PrebuiltLinkOrArchiveTarget *, PrebuiltDep> requirementDeps;
    map<PrebuiltLinkOrArchiveTarget *, PrebuiltDep> usageRequirementDeps;

    map<PrebuiltLinkOrArchiveTarget *, const PrebuiltDep *, IndexInTopologicalSortComparatorRoundZero>
        sortedPrebuiltDependencies;

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
    PrebuiltLinkOrArchiveTarget &DEPS(PrebuiltLinkOrArchiveTarget *dep, Dependency dependency, PrebuiltDep prebuiltDep,
                                      U... deps);

    void populateRequirementAndUsageRequirementDeps();

    PrebuiltLinkOrArchiveTarget(const string &outputName_, const string &directory, TargetType linkTargetType_);
    void preSort(class Builder &builder, unsigned short round) override;
    void updateBTarget(Builder &builder, unsigned short round) override;
    void addRequirementDepsToBTargetDependencies();
    string getActualOutputPath() const;

    template <Dependency dependency, typename T, typename... Property>
    PrebuiltLinkOrArchiveTarget &ASSIGN(T property, Property... properties);
    template <typename T> bool EVALUATE(T property) const;
};
void to_json(Json &json, const PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget);

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::INTERFACE_DEPS(
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget, U... deps)
{
    usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
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
    }
    else if (dependency == Dependency::PRIVATE)
    {
        requirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
    }
    else
    {
        usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
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
    if constexpr (sizeof...(deps))
    {
        return PUBLIC_DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::DEPS(PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget,
                                                               Dependency dependency, PrebuiltDep prebuiltDep,
                                                               U... deps)
{
    if (dependency == Dependency::PUBLIC)
    {
        requirementDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
        usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
    }
    else if (dependency == Dependency::PRIVATE)
    {
        requirementDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
    }
    else
    {
        usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
    }
    if constexpr (sizeof...(deps))
    {
        return DEPS(deps...);
    }
    return *this;
}

template <Dependency dependency, typename T, typename... Property>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::ASSIGN(T property, Property... properties)
{
    if constexpr (std::is_same_v<decltype(property), CopyDLLToExeDirOnNTOs>)
    {
        copyToExeDirOnNtOs = property;
    }
    else
    {
        outputDirectory = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
    if constexpr (sizeof...(properties))
    {
        return ASSIGN(properties...);
    }
    else
    {
        return *this;
    }
}

template <typename T> bool PrebuiltLinkOrArchiveTarget::EVALUATE(T property) const
{
    if constexpr (std::is_same_v<decltype(property), CopyDLLToExeDirOnNTOs>)
    {
        return copyToExeDirOnNtOs == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        return linkTargetType == property;
    }
    else
    {
        outputDirectory = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
}

#endif // HMAKE_PREBUILTLINKORARCHIVETARGET_HPP
