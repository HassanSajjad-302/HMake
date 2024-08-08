
#ifndef HMAKE_PREBUILTLINKORARCHIVETARGET_HPP
#define HMAKE_PREBUILTLINKORARCHIVETARGET_HPP

#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
import "Features.hpp";
import "PrebuiltBasic.hpp";
#else
#include "BTarget.hpp"
#include "Features.hpp"
#include "PrebuiltBasic.hpp"
#endif

class PrebuiltLinkOrArchiveTarget;

class PrebuiltLinkOrArchiveTarget : public PrebuiltBasic, public PrebuiltLinkerFeatures
{
  public:
    pstring outputDirectory;
    pstring actualOutputName;
    pstring usageRequirementLinkerFlags;

    PrebuiltLinkOrArchiveTarget(const pstring &outputName_, const pstring &directory, TargetType linkTargetType_);
    PrebuiltLinkOrArchiveTarget(const pstring &outputName_, const pstring &directory, TargetType linkTargetType_,
                                pstring name_, bool buildExplicit, bool makeDirectory);
    pstring getActualOutputPath() const;

    template <Dependency dependency, typename T, typename... Property>
    PrebuiltLinkOrArchiveTarget &assign(T property, Property... properties);
    template <typename T> bool evaluate(T property) const;
};
void to_json(Json &json, const PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget);

template <Dependency dependency, typename T, typename... Property>
PrebuiltLinkOrArchiveTarget &PrebuiltLinkOrArchiveTarget::assign(T property, Property... properties)
{
    if constexpr (std::is_same_v<decltype(property), CopyDLLToExeDirOnNTOs>)
    {
        copyToExeDirOnNtOs = property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        linkTargetType = property;
    }
    else
    {
        outputDirectory = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
    if constexpr (sizeof...(properties))
    {
        return assign(properties...);
    }
    else
    {
        return *this;
    }
}

template <typename T> bool PrebuiltLinkOrArchiveTarget::evaluate(T property) const
{
    if constexpr (std::is_same_v<decltype(property), CopyDLLToExeDirOnNTOs>)
    {
        return copyToExeDirOnNtOs == property;
    }
    else if constexpr (std::is_same_v<decltype(property), TargetType>)
    {
        return linkTargetType == property;
    }
    else
    {
        outputDirectory = property; // Just to fail the compilation. Ensures that all properties are handled.
    }
}

#endif // HMAKE_PREBUILTLINKORARCHIVETARGET_HPP
