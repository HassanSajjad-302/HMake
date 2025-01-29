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

PrebuiltLinkOrArchiveTarget::PrebuiltLinkOrArchiveTarget(const string &outputName_, string directory,
                                                         TargetType linkTargetType_)
    : PrebuiltBasic(outputName_, linkTargetType_), outputDirectory(std::move(directory))
{
}

PrebuiltLinkOrArchiveTarget::PrebuiltLinkOrArchiveTarget(const string &outputName_, string directory,
                                                         TargetType linkTargetType_, string name_, bool buildExplicit,
                                                         bool makeDirectory)
    : PrebuiltBasic(outputName_, linkTargetType_, std::move(name_), buildExplicit, makeDirectory),
      outputDirectory(std::move(directory))

{
}

void PrebuiltLinkOrArchiveTarget::updateBTarget(Builder &builder, unsigned short round)
{
    PrebuiltBasic::updateBTarget(builder, round);
    if (round == 2)
    {
        actualOutputName = getActualNameFromTargetName(linkTargetType, os, outputName);
        if constexpr (bsMode == BSMode::BUILD)
        {
            if (evaluate(UseMiniTarget::YES))
            {
                readConfigCacheAtBuildTime();
            }
            else
            {
                outputDirectory = Node::getNodeFromNonNormalizedString(outputDirectory, false, true)->filePath;
                outputFileNode =
                    Node::getNodeFromNormalizedString(outputDirectory + slashc + actualOutputName, true, true);
            }
        }
        else
        {
            outputDirectory = Node::getNodeFromNonNormalizedString(outputDirectory, false, true)->filePath;
            outputFileNode = Node::getNodeFromNormalizedString(outputDirectory + slashc + actualOutputName, true, true);

            if (evaluate(UseMiniTarget::YES))
            {
                writeTargetConfigCacheAtConfigureTime();
            }
        }

        for (auto &[prebuiltBasic, prebuiltDep] : requirementDeps)
        {
            if (!prebuiltBasic->evaluate(TargetType::LIBRARY_OBJECT))
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
}

void PrebuiltLinkOrArchiveTarget::writeTargetConfigCacheAtConfigureTime()
{
    buildOrConfigCacheCopy.PushBack(Value(svtogsr(outputDirectory)), cacheAlloc);
    buildOrConfigCacheCopy.PushBack(outputFileNode->getValue(), cacheAlloc);
    copyBackConfigCacheMutexLocked();
}

void PrebuiltLinkOrArchiveTarget::readConfigCacheAtBuildTime()
{
    namespace LinkConfig = Indices::ConfigCache::LinkConfig;
    const Value &v = getConfigCache()[LinkConfig::outputDirectoryNode];
    outputDirectory = string(v.GetString(), v.GetStringLength());
    outputFileNode = Node::getNodeFromValue(getConfigCache()[LinkConfig::outputFileNode], true, true);
}
