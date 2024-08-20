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
    : PrebuiltBasic(outputName_, linkTargetType_), outputDirectoryString(std::move(directory))
{
}

PrebuiltLinkOrArchiveTarget::PrebuiltLinkOrArchiveTarget(const pstring &outputName_, pstring directory,
                                                         TargetType linkTargetType_, pstring name_, bool buildExplicit,
                                                         bool makeDirectory)
    : PrebuiltBasic(outputName_, linkTargetType_, std::move(name_), buildExplicit, makeDirectory),
      outputDirectoryString(std::move(directory))

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
            return;
        }

        outputDirectoryNode = Node::getNodeFromNonNormalizedString(outputDirectoryString, false, true);
        outputFileNode =
            Node::getNodeFromNormalizedString(outputDirectoryNode->filePath + slashc + actualOutputName, true, true);

        if (bsMode == BSMode::CONFIGURE)
        {
            if (evaluate(UseMiniTarget::YES))
            {
                writeTargetConfigCacheAtConfigureTime();
            }
        }
    }
}

void PrebuiltLinkOrArchiveTarget::writeTargetConfigCacheAtConfigureTime() const
{
    targetConfigCache->PushBack(outputDirectoryNode->getPValue(), ralloc);
    targetConfigCache->PushBack(outputFileNode->getPValue(), ralloc);
}

void PrebuiltLinkOrArchiveTarget::readConfigCacheAtBuildTime()
{
    outputDirectoryNode =
        Node::getNodeFromPValue(targetConfigCache[0][Indices::LinkTargetConfigCache::outputDirectoryNode], false, true);
    outputFileNode =
        Node::getNodeFromPValue(targetConfigCache[0][Indices::LinkTargetConfigCache::outputFileNode], true, true);
}
