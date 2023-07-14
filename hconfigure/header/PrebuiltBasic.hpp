#ifndef HMAKE_PREBUILTBASIC_HPP
#define HMAKE_PREBUILTBASIC_HPP

#ifdef USE_HEADER_UNITS
import "BasicTargets.hpp";
import "Features.hpp";
import "FeaturesConvenienceFunctions.hpp";
#else
#include "BasicTargets.hpp"
#include "Features.hpp"
#include "FeaturesConvenienceFunctions.hpp"
#endif

struct PrebuiltDep
{
    // LF linkerFlags

    pstring requirementPreLF;
    pstring usageRequirementPreLF;

    pstring requirementPostLF;
    pstring usageRequirementPostLF;

    pstring requirementRpathLink;
    pstring usageRequirementRpathLink;

    pstring requirementRpath;
    pstring usageRequirementRpath;

    bool defaultRpath = true;
    bool defaultRpathLink = true;
};

class PrebuiltBasic : public BTarget
{
  public:
    pstring outputName;
    vector<const class ObjectFile *> objectFiles;

    map<PrebuiltBasic *, PrebuiltDep> requirementDeps;
    map<PrebuiltBasic *, PrebuiltDep> usageRequirementDeps;

    map<PrebuiltBasic *, const PrebuiltDep *, IndexInTopologicalSortComparatorRoundZero> sortedPrebuiltDependencies;

    set<class ObjectFileProducer *> objectFileProducers;

    TargetType linkTargetType = TargetType::PREBUILT_BASIC;

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
    PrebuiltBasic &DEPS(PrebuiltBasic *dep, Dependency dependency, PrebuiltDep prebuiltDep, U... deps);

    void populateRequirementAndUsageRequirementDeps();

  private:
    void initializePrebuiltBasic();

  public:
    PrebuiltBasic(pstring outputName_);
    PrebuiltBasic(pstring outputName_, TargetType linkTargetType_);

    void preSort(class Builder &builder, unsigned short round) override;
    void updateBTarget(Builder &builder, unsigned short round) override;
    void addRequirementDepsToBTargetDependencies();

    template <Dependency dependency, typename T, typename... Property>
    PrebuiltBasic &ASSIGN(T property, Property... properties);
    template <typename T> bool EVALUATE(T property) const;
};
bool operator<(const PrebuiltBasic &lhs, const PrebuiltBasic &rhs);
void to_json(Json &json, const PrebuiltBasic &prebuiltLinkOrArchiveTarget);

template <typename... U>
PrebuiltBasic &PrebuiltBasic::INTERFACE_DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, U... deps)
{
    usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, PrebuiltDep{});
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
PrebuiltBasic &PrebuiltBasic::INTERFACE_DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, PrebuiltDep prebuiltDep,
                                             U... deps)
{
    usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget, prebuiltDep);
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
    if constexpr (sizeof...(deps))
    {
        return PUBLIC_DEPS(deps...);
    }
    return *this;
}

template <typename... U>
PrebuiltBasic &PrebuiltBasic::DEPS(PrebuiltBasic *prebuiltLinkOrArchiveTarget, Dependency dependency,
                                   PrebuiltDep prebuiltDep, U... deps)
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
PrebuiltBasic &PrebuiltBasic::ASSIGN(T property, Property... properties)
{
    if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        linkTargetType = property;
    }
    else
    {
        linkTargetType = property; // Just to fail the compilation. Ensures that all properties are handled.
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

template <typename T> bool PrebuiltBasic::EVALUATE(T property) const
{
    if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        return linkTargetType == property;
    }
    else
    {
        linkTargetType = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
}

#endif // HMAKE_PREBUILTBASIC_HPP
