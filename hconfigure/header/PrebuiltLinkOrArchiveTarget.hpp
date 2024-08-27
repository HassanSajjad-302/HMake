
#ifndef HMAKE_PREBUILTLINKORARCHIVETARGET_HPP
#define HMAKE_PREBUILTLINKORARCHIVETARGET_HPP

#ifdef USE_HEADER_UNITS
import "PrebuiltBasic.hpp";
#else
#include "PrebuiltBasic.hpp"
#endif

class PrebuiltLinkOrArchiveTarget : public PrebuiltBasic, public PrebuiltLinkerFeatures
{
  public:
    pstring actualOutputName;
    pstring usageRequirementLinkerFlags;
    pstring outputDirectory;
    Node *outputFileNode = nullptr;

    PrebuiltLinkOrArchiveTarget(const pstring &outputName_, pstring directory, TargetType linkTargetType_);
    PrebuiltLinkOrArchiveTarget(const pstring &outputName_, pstring directory, TargetType linkTargetType_,
                                pstring name_, bool buildExplicit, bool makeDirectory);

    template <typename T, typename... Property> PrebuiltLinkOrArchiveTarget &assign(T property, Property... properties);
    template <typename T> bool evaluate(T property) const;
    void updateBTarget(Builder &builder, unsigned short round) override;

private:
    void writeTargetConfigCacheAtConfigureTime() const;
    void readConfigCacheAtBuildTime();
};

template <typename T, typename... Property>
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
    else if constexpr (std::is_same_v<decltype(property), bool>)
    {
        return property;
    }
    else
    {
        return PrebuiltBasic::assign(property);
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
        return PrebuiltBasic::evaluate(property);
    }
}

#endif // HMAKE_PREBUILTLINKORARCHIVETARGET_HPP
