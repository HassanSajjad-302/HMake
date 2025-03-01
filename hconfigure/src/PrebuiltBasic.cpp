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

PrebuiltBasic::PrebuiltBasic(const string &outputName_, const TargetType linkTargetType_)
    : BTarget(outputName_, false, false), TargetCache(outputName_), outputName{getLastNameAfterSlash(outputName_)},
      linkTargetType{linkTargetType_}
{
}

PrebuiltBasic::PrebuiltBasic(const string &outputName_, const TargetType linkTargetType_, const string &name_,
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
            addDependency<0>(*objectFileProducer);
        }
    }
    else if (round == 1)
    {
    }
    else if (round == 2)
    {
        if (evaluate(UseMiniTarget::YES))
        {
            if constexpr (bsMode == BSMode::BUILD)
            {
                readConfigCacheAtBuildTime();
            }
            else
            {
                writeTargetConfigCacheAtConfigureTime();
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
    namespace LinkConfig = Indices::ConfigCache::LinkConfig;

    buildOrConfigCacheCopy.PushBack(kArrayType, cacheAlloc);
    Value &libDirectoriesConfigCache = buildOrConfigCacheCopy[LinkConfig::requirementLibraryDirectoriesArray];
    libDirectoriesConfigCache.Reserve(requirementLibraryDirectories.size(), cacheAlloc);

    for (const LibDirNode &libDirNode : requirementLibraryDirectories)
    {
        libDirectoriesConfigCache.PushBack(libDirNode.node->getValue(), cacheAlloc);
    }

    buildOrConfigCacheCopy.PushBack(kArrayType, cacheAlloc);
    Value &useLibDirectoriesConfigCache = buildOrConfigCacheCopy[LinkConfig::usageRequirementLibraryDirectoriesArray];
    useLibDirectoriesConfigCache.Reserve(usageRequirementLibraryDirectories.size(), cacheAlloc);

    for (const LibDirNode &libDirNode : usageRequirementLibraryDirectories)
    {
        useLibDirectoriesConfigCache.PushBack(libDirNode.node->getValue(), cacheAlloc);
    }

    copyBackConfigCacheMutexLocked();
}

void PrebuiltBasic::readConfigCacheAtBuildTime()
{
    namespace LinkConfig = Indices::ConfigCache::LinkConfig;

    Value &reqLibDirsConfigCache = getConfigCache()[LinkConfig::requirementLibraryDirectoriesArray];
    requirementLibraryDirectories.reserve(reqLibDirsConfigCache.Size());
    for (const Value &pValue : reqLibDirsConfigCache.GetArray())
    {
        requirementLibraryDirectories.emplace_back(Node::getNodeFromValue(pValue, false), true);
    }

    Value &useReqLibDirsConfigCache = getConfigCache()[LinkConfig::usageRequirementLibraryDirectoriesArray];
    usageRequirementLibraryDirectories.reserve(useReqLibDirsConfigCache.Size());
    for (const Value &pValue : useReqLibDirsConfigCache.GetArray())
    {
        usageRequirementLibraryDirectories.emplace_back(Node::getNodeFromValue(pValue, false), true);
    }
}

void PrebuiltBasic::addRequirementDepsToBTargetDependencies()
{
    if (evaluate(TargetType::LIBRARY_STATIC))
    {
        for (auto &[prebuiltBasic, prebuiltDep] : requirementDeps)
        {
            addLooseDependency<0>(*prebuiltBasic);
        }
    }
    else
    {
        for (auto &[prebuiltBasic, prebuiltDep] : requirementDeps)
        {
            addDependency<0>(*prebuiltBasic);
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