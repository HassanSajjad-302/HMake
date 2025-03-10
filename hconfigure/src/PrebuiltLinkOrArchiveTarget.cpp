#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "PrebuiltLinkOrOrArchiveTarget.hpp";
import "SMFile.hpp";
#else
#include "PrebuiltLinkOrArchiveTarget.hpp"

#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "SMFile.hpp"
#include <utility>
#endif

string PrebuiltLinkOrArchiveTarget::getOutputName() const
{
#ifdef BUILD_MODE
    return getTargetNameFromActualName(linkTargetType, os, getLastNameAfterSlash(outputFileNode->filePath));
#else
    return outputName;
#endif
}

string PrebuiltLinkOrArchiveTarget::getActualOutputName() const
{
#ifdef BUILD_MODE
    return getLastNameAfterSlash(outputFileNode->filePath);
#else
    return actualOutputName;
#endif
}

string_view PrebuiltLinkOrArchiveTarget::getOutputDirectoryV() const
{
#ifdef BUILD_MODE
    return getNameBeforeLastSlashV(outputFileNode->filePath);
#else
    return outputDirectory;
#endif
}

#ifdef BUILD_MODE
PrebuiltLinkOrArchiveTarget::PrebuiltLinkOrArchiveTarget(Configuration &config_, const string &outputName_,
                                                         string directory, TargetType linkTargetType_)
    : BTarget(outputName_, false, false), TargetCache(outputName_), config(config_), linkTargetType{linkTargetType_}
{
}

PrebuiltLinkOrArchiveTarget::PrebuiltLinkOrArchiveTarget(Configuration &config_, const string &outputName_,
                                                         string directory, TargetType linkTargetType_, string name_,
                                                         bool buildExplicit, bool makeDirectory)
    : BTarget(name_, buildExplicit, makeDirectory), TargetCache(name_), config(config_), linkTargetType(linkTargetType_)

{
}

#else

PrebuiltLinkOrArchiveTarget::PrebuiltLinkOrArchiveTarget(Configuration &config_, const string &outputName_,
                                                         string directory, TargetType linkTargetType_)
    : BTarget(outputName_, false, false), TargetCache(outputName_), config(config_),
      outputName{getLastNameAfterSlash(outputName_)}, linkTargetType{linkTargetType_},
      outputDirectory(std::move(directory))
{
}

PrebuiltLinkOrArchiveTarget::PrebuiltLinkOrArchiveTarget(Configuration &config_, const string &outputName_,
                                                         string directory, TargetType linkTargetType_, string name_,
                                                         bool buildExplicit, bool makeDirectory)
    : BTarget(name_, buildExplicit, makeDirectory), TargetCache(name_), config(config_), outputName(outputName_),
      linkTargetType(linkTargetType_), outputDirectory(std::move(directory))

{
}

#endif

void PrebuiltLinkOrArchiveTarget::updateBTarget(Builder &builder, unsigned short round)
{
    if (round == 1)
    {
        for (ObjectFileProducer *objectFileProducer : objectFileProducers)
        {
            addDependency<0>(*objectFileProducer);
        }
    }
    else if (round == 2)
    {

        if constexpr (bsMode == BSMode::BUILD)
        {
            readConfigCacheAtBuildTime();
        }
        else
        {

#ifndef BUILD_MODE
            actualOutputName = getActualNameFromTargetName(linkTargetType, os, outputName);
            Node *outputDirectoryNode = Node::getNodeFromNonNormalizedString(outputDirectory, false, true);
            if (outputDirectoryNode->doesNotExist)
            {
                // TODO
                // Throw Exception. Also Replace outputDirectory with outputNode initialized in the constructor.
                // User won't have the ability to setOutputName or setOutputDirectory. Should be achieved in the
                // constructor.

                // throw std::exception(FORMAT("Output directory {} for LinkTarget {} does not exists.",
                // outputDirectoryNode->filePath, name));
            }
            outputDirectory = outputDirectoryNode->filePath;
            outputFileNode = Node::getNodeFromNormalizedString(outputDirectory + slashc + actualOutputName, true, true);

#endif

            writeTargetConfigCacheAtConfigureTime();
        }

        populateRequirementAndUsageRequirementDeps();
        addRequirementDepsToBTargetDependencies();
        for (auto &[PrebuiltLinkOrArchiveTarget, prebuiltDep] : requirementDeps)
        {
            for (const LibDirNode &libDirNode : PrebuiltLinkOrArchiveTarget->usageRequirementLibraryDirectories)
            {
                requirementLibraryDirectories.emplace_back(libDirNode.node, libDirNode.isStandard);
            }
        }

        for (auto &[prebuiltBasic, prebuiltDep] : requirementDeps)
        {
            for (const PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget =
                     static_cast<PrebuiltLinkOrArchiveTarget *>(prebuiltBasic);
                 const LibDirNode &libDirNode : prebuiltLinkOrArchiveTarget->usageRequirementLibraryDirectories)
            {
                requirementLibraryDirectories.emplace_back(libDirNode.node, libDirNode.isStandard);
            }
        }
    }
}

void PrebuiltLinkOrArchiveTarget::writeTargetConfigCacheAtConfigureTime()
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

    buildOrConfigCacheCopy.PushBack(outputFileNode->getValue(), cacheAlloc);
    copyBackConfigCacheMutexLocked();
}

void PrebuiltLinkOrArchiveTarget::readConfigCacheAtBuildTime()
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

    outputFileNode = Node::getNotSystemCheckCalledNodeFromValue(getConfigCache()[LinkConfig::outputFileNode]);
}

void PrebuiltLinkOrArchiveTarget::populateRequirementAndUsageRequirementDeps()
{
    // Set is copied because new elements are to be inserted in it.
    node_hash_map<PrebuiltLinkOrArchiveTarget *, PrebuiltDep> localRequirementDeps = requirementDeps;

    for (auto &[PrebuiltLinkOrArchiveTarget, prebuiltDep] : localRequirementDeps)
    {
        for (auto &[PrebuiltLinkOrArchiveTarget_, prebuilt] : PrebuiltLinkOrArchiveTarget->usageRequirementDeps)
        {
            PrebuiltDep prebuiltDep_;

            prebuiltDep_.requirementPreLF = prebuilt.usageRequirementPreLF;
            prebuiltDep_.requirementPostLF = prebuilt.usageRequirementPostLF;
            prebuiltDep_.requirementRpathLink = prebuilt.usageRequirementRpathLink;
            prebuiltDep_.requirementRpath = prebuilt.usageRequirementRpath;
            prebuiltDep_.defaultRpath = prebuilt.defaultRpath;
            prebuiltDep_.defaultRpathLink = prebuilt.defaultRpathLink;

            requirementDeps.emplace(PrebuiltLinkOrArchiveTarget_, std::move(prebuiltDep_));
        }
    }

    for (auto localUsageRequirements = usageRequirementDeps;
         auto &[PrebuiltLinkOrArchiveTarget, prebuiltDep] : localUsageRequirements)
    {
        for (auto &[PrebuiltLinkOrArchiveTarget_, prebuilt] : PrebuiltLinkOrArchiveTarget->usageRequirementDeps)
        {
            PrebuiltDep prebuiltDep_;

            prebuiltDep_.usageRequirementPreLF = prebuilt.usageRequirementPreLF;
            prebuiltDep_.usageRequirementPostLF = prebuilt.usageRequirementPostLF;
            prebuiltDep_.usageRequirementRpathLink = prebuilt.usageRequirementRpathLink;
            prebuiltDep_.usageRequirementRpath = prebuilt.usageRequirementRpath;
            prebuiltDep_.defaultRpath = prebuilt.defaultRpath;
            prebuiltDep_.defaultRpathLink = prebuilt.defaultRpathLink;

            usageRequirementDeps.emplace(PrebuiltLinkOrArchiveTarget_, std::move(prebuiltDep_));
        }
    }
}

void PrebuiltLinkOrArchiveTarget::addRequirementDepsToBTargetDependencies()
{
    if (evaluate(TargetType::LIBRARY_STATIC))
    {
        for (auto &[PrebuiltLinkOrArchiveTarget, prebuiltDep] : requirementDeps)
        {
            addLooseDependency<0>(*PrebuiltLinkOrArchiveTarget);
        }
    }
    else
    {
        for (auto &[PrebuiltLinkOrArchiveTarget, prebuiltDep] : requirementDeps)
        {
            addDependency<0>(*PrebuiltLinkOrArchiveTarget);
        }
    }
}

bool operator<(const PrebuiltLinkOrArchiveTarget &lhs, const PrebuiltLinkOrArchiveTarget &rhs)
{
    return lhs.id < rhs.id;
}

void to_json(Json &json, const PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget)
{
    json = PrebuiltLinkOrArchiveTarget.getTarjanNodeName();
}
