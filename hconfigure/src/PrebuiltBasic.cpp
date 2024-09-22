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
    node_hash_map<PrebuiltBasic *, PrebuiltDep> localRequirementDeps = requirementDeps;

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

void PrebuiltBasic::initializePrebuiltBasic(const pstring &name_)
{
    const uint64_t index = pvalueIndexInSubArrayConsidered(tempCache, PValue(ptoref(name_)));

    if (bsMode == BSMode::CONFIGURE)
    {
        if (index == UINT64_MAX)
        {
            tempCache.PushBack(PValue(kArrayType), ralloc);
            targetTempCache = &tempCache[tempCache.Size() - 1];
            targetTempCache->PushBack(PValue(kStringType).SetString(name_.c_str(), name_.size(), ralloc), ralloc);
            targetTempCache->PushBack(PValue(kArrayType), ralloc);
            targetTempCache->PushBack(PValue(kArrayType), ralloc);
        }
        else
        {
            targetTempCache = &tempCache[index];
            (*targetTempCache)[Indices::CppTarget::configCache].Clear();
        }
    }
    else
    {
        if (index != UINT64_MAX)
        {
            targetTempCache = &tempCache[index];
        }
        else
        {
            printErrorMessage(fmt::format("Target {} not found in build-cache\n", name));
            exit(EXIT_FAILURE);
        }
    }
}

PrebuiltBasic::PrebuiltBasic(const pstring &outputName_, const TargetType linkTargetType_)
    : BTarget(outputName_, false, false), outputName{getLastNameAfterSlash(outputName_)},
      linkTargetType{linkTargetType_}
{
    initializePrebuiltBasic(outputName_);
}

PrebuiltBasic::PrebuiltBasic(pstring outputName_, const TargetType linkTargetType_, const pstring &name_,
                             const bool buildExplicit, const bool makeDirectory)
    : BTarget(name_, buildExplicit, makeDirectory), outputName(std::move(outputName_)), linkTargetType(linkTargetType_)
{
    initializePrebuiltBasic(name_);
}

void PrebuiltBasic::updateBTarget(Builder &, const unsigned short round)
{
    if (round == 1)
    {
        for (ObjectFileProducer *objectFileProducer : objectFileProducers)
        {
            realBTargets[0].addDependency(*objectFileProducer);
        }
    }
    else if (round == 1)
    {
    }
    else if (round == 2)
    {
        if (bsMode == BSMode::BUILD && evaluate(UseMiniTarget::YES))
        {
            readConfigCacheAtBuildTime();
        }
        else
        {
            if (bsMode == BSMode::CONFIGURE)
            {
                if (evaluate(UseMiniTarget::YES))
                {
                    writeTargetConfigCacheAtConfigureTime();
                }
            }
        }

        populateRequirementAndUsageRequirementDeps();
        addRequirementDepsToBTargetDependencies();
        for (auto &[prebuiltBasic, prebuiltDep] : requirementDeps)
        {
            for (const LibDirNode &libDirNode : prebuiltBasic->usageRequirementLibraryDirectories)
            {
                requirementLibraryDirectories.emplace_back(libDirNode.node, libDirNode.isStandard);
            }
        }
    }
}

void PrebuiltBasic::writeTargetConfigCacheAtConfigureTime() const
{
    namespace LinkTarget = Indices::LinkTarget;
    PValue &targetConfigCache = (*targetTempCache)[LinkTarget::configCache];

    targetConfigCache.PushBack(kArrayType, ralloc);
    PValue &libDirectoriesConfigCache = targetConfigCache[LinkTarget::ConfigCache::requirementLibraryDirectoriesArray];
    libDirectoriesConfigCache.Reserve(requirementLibraryDirectories.size(), ralloc);

    for (const LibDirNode &libDirNode : requirementLibraryDirectories)
    {
        libDirectoriesConfigCache.PushBack(libDirNode.node->getPValue(), ralloc);
    }

    targetConfigCache.PushBack(kArrayType, ralloc);
    PValue &useLibDirectoriesConfigCache =
        targetConfigCache[LinkTarget::ConfigCache::usageRequirementLibraryDirectoriesArray];
    useLibDirectoriesConfigCache.Reserve(usageRequirementLibraryDirectories.size(), ralloc);

    for (const LibDirNode &libDirNode : usageRequirementLibraryDirectories)
    {
        useLibDirectoriesConfigCache.PushBack(libDirNode.node->getPValue(), ralloc);
    }
}

void PrebuiltBasic::readConfigCacheAtBuildTime()
{
    namespace LinkTarget = Indices::LinkTarget;
    PValue &targetConfigCache = (*targetTempCache)[LinkTarget::configCache];

    PValue &reqLibDirsConfigCache = targetConfigCache[LinkTarget::ConfigCache::requirementLibraryDirectoriesArray];
    requirementLibraryDirectories.reserve(reqLibDirsConfigCache.Size());
    for (const PValue &pValue : reqLibDirsConfigCache.GetArray())
    {
        requirementLibraryDirectories.emplace_back(Node::getNodeFromPValue(pValue, false), true);
    }

    PValue &useReqLibDirsConfigCache =
        targetConfigCache[LinkTarget::ConfigCache::usageRequirementLibraryDirectoriesArray];
    usageRequirementLibraryDirectories.reserve(useReqLibDirsConfigCache.Size());
    for (const PValue &pValue : useReqLibDirsConfigCache.GetArray())
    {
        usageRequirementLibraryDirectories.emplace_back(Node::getNodeFromPValue(pValue, false), true);
    }
}

void PrebuiltBasic::addRequirementDepsToBTargetDependencies()
{
    RealBTarget &round0 = realBTargets[0];
    if (evaluate(TargetType::LIBRARY_STATIC))
    {
        for (auto &[prebuiltBasic, prebuiltDep] : requirementDeps)
        {
            round0.addLooseDependency(*prebuiltBasic);
        }
    }
    else
    {
        for (auto &[prebuiltBasic, prebuiltDep] : requirementDeps)
        {
            round0.addDependency(*prebuiltBasic);
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