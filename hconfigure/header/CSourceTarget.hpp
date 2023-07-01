
#ifndef HMAKE_CSOURCETARGET_HPP
#define HMAKE_CSOURCETARGET_HPP

#ifdef USE_HEADER_UNITS
import "SMFile.hpp";
import <set>;
#else
#include "SMFile.hpp"
#include <set>
#endif

using std::set;

// CPrebuiltTarget
struct CSourceTarget : public ObjectFileProducerWithDS<CSourceTarget>
{
    using BaseType = CSourceTarget;
    list<InclNode> usageRequirementIncludes;
    pstring usageRequirementCompilerFlags;
    set<struct Define> usageRequirementCompileDefinitions;

    template <typename... U> CSourceTarget &INTERFACE_INCLUDES(const pstring &include, U... includeDirectoryPString);
    CSourceTarget &INTERFACE_COMPILER_FLAGS(const pstring &compilerFlags);
    CSourceTarget &INTERFACE_COMPILE_DEFINITION(const pstring &cddName, const pstring &cddValue = "");
};
bool operator<(const CSourceTarget &lhs, const CSourceTarget &rhs);

template <typename... U>
CSourceTarget &CSourceTarget::INTERFACE_INCLUDES(const pstring &include, U... includeDirectoryPString)
{
    InclNode::emplaceInList(usageRequirementIncludes, Node::getNodeFromPath(include, false));
    if constexpr (sizeof...(includeDirectoryPString))
    {
        return INTERFACE_INCLUDES(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

#endif // HMAKE_CSOURCETARGET_HPP
