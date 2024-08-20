
#ifndef HMAKE_CSOURCETARGET_HPP
#define HMAKE_CSOURCETARGET_HPP

#ifdef USE_HEADER_UNITS
impoert "Features.hpp";
import "ObjectFileProducer.hpp";
import "SpecialNodes.hpp";
import <set>;
#else
#include "Features.hpp"
#include "ObjectFileProducer.hpp"
#include "SpecialNodes.hpp"
#include <set>
#endif

using std::set;

enum class CSourceTargetType : unsigned short
{
    CSourceTarget = 1,
    CppSourceTarget = 2,
};

struct CSourceTarget : ObjectFileProducerWithDS<CSourceTarget>
{
    using BaseType = CSourceTarget;
    vector<InclNode> usageRequirementIncludes;
    pstring usageRequirementCompilerFlags;
    set<Define> usageRequirementCompileDefinitions;
    PValue *targetConfigCache = nullptr;

    explicit CSourceTarget(bool buildExplicit, pstring name);
    template <typename... U> CSourceTarget &interfaceIncludes(const pstring &include, U... includeDirectoryPString);
    CSourceTarget &INTERFACE_COMPILER_FLAGS(const pstring &compilerFlags);
    CSourceTarget &INTERFACE_COMPILE_DEFINITION(const pstring &cddName, const pstring &cddValue = "");
    virtual CSourceTargetType getCSourceTargetType() const;
};
bool operator<(const CSourceTarget &lhs, const CSourceTarget &rhs);

template <typename... U>
CSourceTarget &CSourceTarget::interfaceIncludes(const pstring &include, U... includeDirectoryPString)
{
    if (bsMode == BSMode::BUILD && useMiniTarget == UseMiniTarget::YES)
    {
        // Initialized in CppSourceTarget round 2
    }
    else
    {
        CppCompilerFeatures::actuallyAddInclude(usageRequirementIncludes, include, false);
    }

    if constexpr (sizeof...(includeDirectoryPString))
    {
        return interfaceIncludes(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

#endif // HMAKE_CSOURCETARGET_HPP
