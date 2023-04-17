#ifdef USE_HEADER_UNITS
import "PrebuiltLinkOrArchiveTarget.hpp";
#else
#include "PrebuiltLinkOrArchiveTarget.hpp"
#endif

PrebuiltDep::PrebuiltDep(bool defaultRPath_, bool defaultRpathLink_)
    : defaultRpath{defaultRPath_}, defaultRpathLink{defaultRpathLink_}
{
}

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
            prebuiltDep_.requirementRpath = prebuilt.usageRequirementRPath;
            prebuiltDep_.defaultRpath = false;
            prebuiltDep_.defaultRpathLink = false;

            requirementDeps.emplace(prebuiltLinkOrArchiveTarget, std::move(prebuiltDep_));
        }
    }

    for (auto &[prebuiltLinkOrArchiveTarget, prebuiltDep] : usageRequirementDeps)
    {
        for (auto &[prebuiltLinkOrArchiveTarget_, prebuilt] : prebuiltLinkOrArchiveTarget->usageRequirementDeps)
        {
            usageRequirementDeps.emplace(prebuiltLinkOrArchiveTarget_, PrebuiltDep{});
        }
    }
}

PrebuiltLinkOrArchiveTarget::PrebuiltLinkOrArchiveTarget(const string &name, const string &directory,
                                                         TargetType linkTargetType_)
    : linkTargetType(linkTargetType_), outputDirectory(Node::getFinalNodePathFromString(directory).string()),
      outputName(name), actualOutputName(getActualNameFromTargetName(linkTargetType_, os, name))
{
}

void PrebuiltLinkOrArchiveTarget::preSort(Builder &builder, unsigned short round)
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

string PrebuiltLinkOrArchiveTarget::getActualOutputPath()
{
    return outputDirectory + actualOutputName;
}

void to_json(Json &json, const PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget)
{
    json = prebuiltLinkOrArchiveTarget.getTarjanNodeName();
}
