#ifdef USE_HEADER_UNITS
import "PrebuiltLinkOrArchiveTarget.hpp";
import "SMFile.hpp";
#else
#include "PrebuiltLinkOrArchiveTarget.hpp"
#include "SMFile.hpp"
#endif

void PrebuiltLinkOrArchiveTarget::populateRequirementAndUsageRequirementDeps()
{
    // Set is copied because new elements are to be inserted in it.
    map<PrebuiltLinkOrArchiveTarget *, PrebuiltDep> localRequirementDeps = requirementDeps;

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

PrebuiltLinkOrArchiveTarget::PrebuiltLinkOrArchiveTarget(const string &outputName_, const string &directory,
                                                         TargetType linkTargetType_)
    : outputDirectory(Node::getFinalNodePathFromString(directory).string()), outputName(outputName_),
      actualOutputName(getActualNameFromTargetName(linkTargetType_, os, outputName_)), linkTargetType(linkTargetType_)
{
}

void PrebuiltLinkOrArchiveTarget::preSort(Builder &, unsigned short round)
{
    if (round == 3)
    {
        RealBTarget &round3 = getRealBTarget(3);
        for (auto &[prebuiltLinkOrArchiveTarget, prebuiltDep] : requirementDeps)
        {
            round3.addDependency(const_cast<PrebuiltLinkOrArchiveTarget &>(*prebuiltLinkOrArchiveTarget));
        }
        for (auto &[prebuiltLinkOrArchiveTarget, prebuiltDep] : usageRequirementDeps)
        {
            round3.addDependency(const_cast<PrebuiltLinkOrArchiveTarget &>(*prebuiltLinkOrArchiveTarget));
        }
        getRealBTarget(3).fileStatus = FileStatus::NEEDS_UPDATE;
    }
}

void PrebuiltLinkOrArchiveTarget::updateBTarget(Builder &, unsigned short round)
{
    if (round == 3)
    {
        populateRequirementAndUsageRequirementDeps();
        addRequirementDepsToBTargetDependencies();
    }
}

void PrebuiltLinkOrArchiveTarget::addRequirementDepsToBTargetDependencies()
{
    // Access to addDependency() function must be synchronized because set::emplace is not thread-safe
    std::lock_guard<std::mutex> lk(BTargetNamespace::addDependencyMutex);
    RealBTarget &round0 = getRealBTarget(0);
    RealBTarget &round2 = getRealBTarget(2);
    for (auto &[prebuiltLinkOrArchiveTarget, prebuiltDep] : requirementDeps)
    {
        round0.addDependency(const_cast<PrebuiltLinkOrArchiveTarget &>(*prebuiltLinkOrArchiveTarget));
        round2.addDependency(const_cast<PrebuiltLinkOrArchiveTarget &>(*prebuiltLinkOrArchiveTarget));
    }
}

string PrebuiltLinkOrArchiveTarget::getActualOutputPath() const
{
    return outputDirectory + actualOutputName;
}

void to_json(Json &json, const PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget)
{
    json = prebuiltLinkOrArchiveTarget.getTarjanNodeName();
}
