
#ifndef HMAKE_CSOURCETARGET_HPP
#define HMAKE_CSOURCETARGET_HPP

#ifdef USE_HEADER_UNITS
import "SMFile.hpp";
import <set>;
import <string>;
#else
#include "SMFile.hpp"
#include <set>
#include <string>
#endif

using std::set, std::string;

// CPrebuiltTarget
struct CSourceTarget : public ObjectFileProducerWithDS<CSourceTarget>
{
    using BaseType = CSourceTarget;
    list<InclNode> usageRequirementIncludes;
    string usageRequirementCompilerFlags;
    set<struct Define> usageRequirementCompileDefinitions;

    template <typename... U> CSourceTarget &INTERFACE_INCLUDES(const string &include, U... includeDirectoryString);
    CSourceTarget &INTERFACE_COMPILER_FLAGS(const string &compilerFlags);
    CSourceTarget &INTERFACE_COMPILE_DEFINITION(const string &cddName, const string &cddValue = "");
};

template <typename... U>
CSourceTarget &CSourceTarget::INTERFACE_INCLUDES(const string &include, U... includeDirectoryString)
{
    InclNode::emplaceInList(usageRequirementIncludes, Node::getNodeFromString(include, false));
    if constexpr (sizeof...(includeDirectoryString))
    {
        return INTERFACE_INCLUDES(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

#endif // HMAKE_CSOURCETARGET_HPP
