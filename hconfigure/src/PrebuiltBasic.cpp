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

    for (auto &[prebuiltBasic, prebuiltDep] : localRequirementDeps)
    {
        for (auto &[prebuiltBasic_, prebuilt] : prebuiltBasic->usageRequirementDeps)
        {
            PrebuiltDep prebuiltDep_;

            prebuiltDep_.requirementPreLF = prebuilt.usageRequirementPreLF;
            prebuiltDep_.requirementPostLF = prebuilt.usageRequirementPostLF;
            prebuiltDep_.requirementRpathLink = prebuilt.usageRequirementRpathLink;
            prebuiltDep_.requirementRpath = prebuilt.usageRequirementRpath;
            prebuiltDep_.defaultRpath = prebuilt.defaultRpath;
            prebuiltDep_.defaultRpathLink = prebuilt.defaultRpathLink;

            requirementDeps.emplace(prebuiltBasic_, std::move(prebuiltDep_));
        }
    }

    for (auto &[prebuiltBasic, prebuiltDep] : usageRequirementDeps)
    {
        for (auto &[prebuiltBasic_, prebuilt] : prebuiltBasic->usageRequirementDeps)
        {
            PrebuiltDep prebuiltDep_;

            prebuiltDep_.usageRequirementPreLF = prebuilt.usageRequirementPreLF;
            prebuiltDep_.usageRequirementPostLF = prebuilt.usageRequirementPostLF;
            prebuiltDep_.usageRequirementRpathLink = prebuilt.usageRequirementRpathLink;
            prebuiltDep_.usageRequirementRpath = prebuilt.usageRequirementRpath;
            prebuiltDep_.defaultRpath = prebuilt.defaultRpath;
            prebuiltDep_.defaultRpathLink = prebuilt.defaultRpathLink;

            usageRequirementDeps.emplace(prebuiltBasic_, std::move(prebuiltDep_));
        }
    }
}

void PrebuiltBasic::initializePrebuiltBasic()
{
    preSortBTargets.emplace_back(this);
    realBTargets.emplace_back(this, 0);
    realBTargets.emplace_back(this, 1);
    realBTargets.emplace_back(this, 2);
}

PrebuiltBasic::PrebuiltBasic(pstring outputName_) : outputName{std::move(outputName_)}
{
    initializePrebuiltBasic();
}

PrebuiltBasic::PrebuiltBasic(pstring outputName_, TargetType linkTargetType_)
    : outputName{std::move(outputName_)}, linkTargetType{linkTargetType_}
{
    initializePrebuiltBasic();
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
        RealBTarget &round2 = realBTargets[2];
        for (auto &[prebuiltBasic, prebuiltDep] : requirementDeps)
        {
            round2.addDependency(const_cast<PrebuiltBasic &>(*prebuiltBasic));
        }
        for (auto &[prebuiltBasic, prebuiltDep] : usageRequirementDeps)
        {
            round2.addDependency(const_cast<PrebuiltBasic &>(*prebuiltBasic));
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
    RealBTarget &round0 = realBTargets[0];
    if (EVALUATE(TargetType::LIBRARY_STATIC))
    {
        for (auto &[prebuiltBasic, prebuiltDep] : requirementDeps)
        {
            round0.addLooseDependency(const_cast<PrebuiltBasic &>(*prebuiltBasic));
        }
    }
    else
    {
        for (auto &[prebuiltBasic, prebuiltDep] : requirementDeps)
        {
            round0.addDependency(const_cast<PrebuiltBasic &>(*prebuiltBasic));
        }
    }
}

bool operator<(const PrebuiltBasic &lhs, const PrebuiltBasic &rhs)
{
    return lhs.id < rhs.id;
}

void to_json(Json &json, const PrebuiltBasic &prebuiltBasic)
{
    json = prebuiltBasic.getTarjanNodeName();
}