#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp";
import "Builder.hpp";
import "PrebuiltLinkOrArchiveTarget.hpp";
import "SMFile.hpp";
#else
#include "PrebuiltLinkOrArchiveTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "SMFile.hpp"
#endif

// TODO
// public constructor should check the existence of library but the protected constructor shouldn't which will be used
// by LinkOrArchiveTarget.
PrebuiltLinkOrArchiveTarget::PrebuiltLinkOrArchiveTarget(const pstring &outputName_, const pstring &directory,
                                                         const TargetType linkTargetType_)
    : PrebuiltBasic(outputName_, linkTargetType_),
      outputDirectory((Node::getFinalNodePathFromPath(directory + slashc).*toPStr)()),
      actualOutputName(getActualNameFromTargetName(linkTargetType_, os, outputName_))
{
}

PrebuiltLinkOrArchiveTarget::PrebuiltLinkOrArchiveTarget(const pstring &outputName_, const pstring &directory,
                                                         TargetType linkTargetType_, pstring name_, bool buildExplicit,
                                                         bool makeDirectory)
    : PrebuiltBasic(outputName_, linkTargetType_, std::move(name_), buildExplicit, makeDirectory),
      outputDirectory((Node::getFinalNodePathFromPath(directory + slashc).*toPStr)()),
      actualOutputName(getActualNameFromTargetName(linkTargetType_, os, outputName_))
{
}

pstring PrebuiltLinkOrArchiveTarget::getActualOutputPath() const
{
    return outputDirectory + actualOutputName;
}

void to_json(Json &json, const PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget)
{
    json = prebuiltLinkOrArchiveTarget.getTarjanNodeName();
}
