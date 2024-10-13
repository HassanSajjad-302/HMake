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

    for (auto localUsageRequirements = usageRequirementDeps;
         auto &[prebuiltBasic, prebuiltDep] : localUsageRequirements)
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

PrebuiltBasic::PrebuiltBasic(const pstring &outputName_, const TargetType linkTargetType_)
    : BTarget(outputName_, false, false), TargetCache(outputName_), outputName{getLastNameAfterSlash(outputName_)},
      linkTargetType{linkTargetType_}
{
}

PrebuiltBasic::PrebuiltBasic(const pstring &outputName_, const TargetType linkTargetType_, const pstring &name_,
                             const bool buildExplicit, const bool makeDirectory)
    : BTarget(name_, buildExplicit, makeDirectory), TargetCache(name_), outputName(outputName_),
      linkTargetType(linkTargetType_)
{
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

void PrebuiltBasic::writeTargetConfigCacheAtConfigureTime()
{
    namespace ConfigCache = Indices::LinkTarget::ConfigCache;

    buildOrConfigCacheCopy.PushBack(kArrayType, cacheAlloc);
    PValue &libDirectoriesConfigCache = buildOrConfigCacheCopy[ConfigCache::requirementLibraryDirectoriesArray];
    libDirectoriesConfigCache.Reserve(requirementLibraryDirectories.size(), cacheAlloc);

    for (const LibDirNode &libDirNode : requirementLibraryDirectories)
    {
        libDirectoriesConfigCache.PushBack(libDirNode.node->getPValue(), cacheAlloc);
    }

    buildOrConfigCacheCopy.PushBack(kArrayType, cacheAlloc);
    PValue &useLibDirectoriesConfigCache = buildOrConfigCacheCopy[ConfigCache::usageRequirementLibraryDirectoriesArray];
    useLibDirectoriesConfigCache.Reserve(usageRequirementLibraryDirectories.size(), cacheAlloc);

    for (const LibDirNode &libDirNode : usageRequirementLibraryDirectories)
    {
        useLibDirectoriesConfigCache.PushBack(libDirNode.node->getPValue(), cacheAlloc);
    }

    copyBackConfigCacheMutexLocked();
}

void PrebuiltBasic::readConfigCacheAtBuildTime()
{
    namespace ConfigCache = Indices::LinkTarget::ConfigCache;

    PValue &reqLibDirsConfigCache = getConfigCache()[ConfigCache::requirementLibraryDirectoriesArray];
    requirementLibraryDirectories.reserve(reqLibDirsConfigCache.Size());
    for (const PValue &pValue : reqLibDirsConfigCache.GetArray())
    {
        requirementLibraryDirectories.emplace_back(Node::getNodeFromPValue(pValue, false), true);
    }

    PValue &useReqLibDirsConfigCache = getConfigCache()[ConfigCache::usageRequirementLibraryDirectoriesArray];
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