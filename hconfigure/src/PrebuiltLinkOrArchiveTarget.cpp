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

PrebuiltLinkOrArchiveTarget::PrebuiltLinkOrArchiveTarget(const pstring &outputName_, pstring directory,
                                                         TargetType linkTargetType_)
    : PrebuiltBasic(outputName_, linkTargetType_), outputDirectory(std::move(directory))
{
}

PrebuiltLinkOrArchiveTarget::PrebuiltLinkOrArchiveTarget(const pstring &outputName_, pstring directory,
                                                         TargetType linkTargetType_, pstring name_, bool buildExplicit,
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
        if (bsMode == BSMode::BUILD && evaluate(UseMiniTarget::YES))
        {
            readConfigCacheAtBuildTime();
        }
        else
        {
            outputDirectory = Node::getNodeFromNonNormalizedString(outputDirectory, false, true)->filePath;
            outputFileNode = Node::getNodeFromNormalizedString(outputDirectory + slashc + actualOutputName, true, true);

            if (bsMode == BSMode::CONFIGURE)
            {
                if (evaluate(UseMiniTarget::YES))
                {
                    writeTargetConfigCacheAtConfigureTime();
                }
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

void PrebuiltLinkOrArchiveTarget::writeTargetConfigCacheAtConfigureTime() const
{
    targetConfigCache->PushBack(PValue(ptoref(outputDirectory)), ralloc);
    targetConfigCache->PushBack(outputFileNode->getPValue(), ralloc);
}

void PrebuiltLinkOrArchiveTarget::readConfigCacheAtBuildTime()
{
    const PValue &v = targetConfigCache[0][Indices::LinkTargetConfigCache::outputDirectoryNode];
    outputDirectory = pstring(v.GetString(), v.GetStringLength());
    outputFileNode =
        Node::getNodeFromPValue(targetConfigCache[0][Indices::LinkTargetConfigCache::outputFileNode], true, true);
}
