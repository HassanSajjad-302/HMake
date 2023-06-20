#ifdef USE_HEADER_UNITS
import "Builder.hpp";
import "PrebuiltBasic.hpp";
import "SMFile.hpp";
#else
#include "PrebuiltBasic.hpp"

#include "Builder.hpp"
#include "SMFile.hpp"
#include <utility>
#endif

void PrebuiltBasic::populateRequirementAndUsageRequirementDeps()
{
    // Set is copied because new elements are to be inserted in it.
    map<PrebuiltBasic *, PrebuiltDep> localRequirementDeps = requirementDeps;

    for (auto &[prebuiltLinkOrArchiveTarget, prebuiltDep] : localRequirementDeps)
    {
        for (auto &[prebuiltLinkOrArchiveTarget_, prebuilt] : prebuiltLinkOrArchiveTarget->usageRequirementDeps)
        {
            PrebuiltDep prebuiltDep_;

            prebuiltDep_.requirementPreLF = prebuilt.usageRequirementPreLF;
            prebuiltDep_.requirementPostLF = prebuilt.usageRequirementPostLF;
            prebuiltDep_.requirementRpathLink = prebuilt.usageRequirementRpathLink;
            prebuiltDep_.requirementRpath = prebuilt.usageRequirementRpath;
            prebuiltDep_.defaultRpath = prebuilt.defaultRpath;
            prebuiltDep_.defaultRpathLink = prebuilt.defaultRpathLink;

            requirementDeps.emplace(prebuiltLinkOrArchiveTarget_, std::move(prebuiltDep_));
        }
    }

    for (auto &[prebuiltLinkOrArchiveTarget, prebuiltDep] : usageRequirementDeps)
    {
        for (auto &[prebuiltLinkOrArchiveTarget_, prebuilt] : prebuiltLinkOrArchiveTarget->usageRequirementDeps)
        {
            PrebuiltDep prebuiltDep_;

            prebuiltDep_.usageRequirementPreLF = prebuilt.usageRequirementPreLF;
            prebuiltDep_.usageRequirementPostLF = prebuilt.usageRequirementPostLF;
            prebuiltDep_.usageRequirementRpathLink = prebuilt.usageRequirementRpathLink;
            prebuiltDep_.usageRequirementRpath = prebuilt.usageRequirementRpath;
            prebuiltDep_.defaultRpath = prebuilt.defaultRpath;
            prebuiltDep_.defaultRpathLink = prebuilt.defaultRpathLink;

            usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget_, std::move(prebuiltDep_));
        }
    }
}

PrebuiltBasic::PrebuiltBasic(string outputName_) : outputName{std::move(outputName_)}
{
    preSortBTargets.emplace_back(this);
}

PrebuiltBasic::PrebuiltBasic(string outputName_, TargetType linkTargetType_)
    : outputName{std::move(outputName_)}, linkTargetType{linkTargetType_}
{
    preSortBTargets.emplace_back(this);
}

void PrebuiltBasic::preSort(Builder &, unsigned short round)
{
    if (!round)
    {
        for (ObjectFileProducer *objectFileProducer : objectFileProducers)
        {
            objectFileProducer->addDependencyOnObjectFileProducers(this);
        }
    }
    else if (round == 2)
    {
        RealBTarget &round2 = getRealBTarget(2);
        for (auto &[prebuiltLinkOrArchiveTarget, prebuiltDep] : requirementDeps)
        {
            round2.addDependency(const_cast<PrebuiltBasic &>(*prebuiltLinkOrArchiveTarget));
        }
        for (auto &[prebuiltLinkOrArchiveTarget, prebuiltDep] : usageRequirementDeps)
        {
            round2.addDependency(const_cast<PrebuiltBasic &>(*prebuiltLinkOrArchiveTarget));
        }
    }
}

void PrebuiltBasic::updateBTarget(Builder &, unsigned short round)
{
    if (round == 2)
    {
        populateRequirementAndUsageRequirementDeps();
        addRequirementDepsToBTargetDependencies();
    }
}

void PrebuiltBasic::addRequirementDepsToBTargetDependencies()
{
    // Access to addDependency() function must be synchronized because set::emplace is not thread-safe
    std::lock_guard<std::mutex> lk(BTargetNamespace::addDependencyMutex);
    RealBTarget &round0 = getRealBTarget(0);
    if (EVALUATE(TargetType::LIBRARY_STATIC))
    {
        for (auto &[prebuiltLinkOrArchiveTarget, prebuiltDep] : requirementDeps)
        {
            round0.addLooseDependency(const_cast<PrebuiltBasic &>(*prebuiltLinkOrArchiveTarget));
        }
    }
    else
    {
        for (auto &[prebuiltLinkOrArchiveTarget, prebuiltDep] : requirementDeps)
        {
            round0.addDependency(const_cast<PrebuiltBasic &>(*prebuiltLinkOrArchiveTarget));
        }
    }
}

bool operator<(const PrebuiltBasic &lhs, const PrebuiltBasic &rhs)
{
    return lhs.id < rhs.id;
}

void to_json(Json &json, const PrebuiltBasic &prebuiltLinkOrArchiveTarget)
{
    json = prebuiltLinkOrArchiveTarget.getTarjanNodeName();
}