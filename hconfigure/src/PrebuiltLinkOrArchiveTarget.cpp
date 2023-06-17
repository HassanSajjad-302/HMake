#ifdef USE_HEADER_UNITS
import "Builder.hpp";
import "PrebuiltLinkOrArchiveTarget.hpp";
import "SMFile.hpp";
#else
#include "PrebuiltLinkOrArchiveTarget.hpp"
#include "Builder.hpp"
#include "SMFile.hpp"
#endif

// TODO
// public constructor should check the existence of library but the protected constructor shouldn't which will be used
// by LinkOrArchiveTarget.
PrebuiltLinkOrArchiveTarget::PrebuiltLinkOrArchiveTarget(const string &outputName_, const string &directory,
                                                         TargetType linkTargetType_)
    : outputDirectory(Node::getFinalNodePathFromString(directory).string()),
      actualOutputName(getActualNameFromTargetName(linkTargetType_, os, outputName_)),
      PrebuiltBasic(outputName_, linkTargetType_)
{
}

string PrebuiltLinkOrArchiveTarget::getActualOutputPath() const
{
    return outputDirectory + actualOutputName;
}

void to_json(Json &json, const PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget)
{
    json = prebuiltLinkOrArchiveTarget.getTarjanNodeName();
}
