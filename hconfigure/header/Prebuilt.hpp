
#ifndef HMAKE_PREBUILT_HPP
#define HMAKE_PREBUILT_HPP

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
struct CPT : public ObjectFileProducerWithDS<CPT>
{
    map<const class Node *, class InclNode> usageRequirementIncludes;
    string usageRequirementCompilerFlags;
    set<struct Define> usageRequirementCompileDefinitions;

    template <typename... U> CPT &INTERFACE_INCLUDES(const string &include, U... includeDirectoryString);
    CPT &INTERFACE_COMPILER_FLAGS(const string &compilerFlags);
    CPT &INTERFACE_COMPILE_DEFINITION(const string &cddName, const string &cddValue = "");
};

template <typename... U> CPT &CPT::INTERFACE_INCLUDES(const string &include, U... includeDirectoryString)
{
    usageRequirementIncludes.emplace(Node::getNodeFromString(include, false), InclNode(false, false));
    if constexpr (sizeof...(includeDirectoryString))
    {
        return INTERFACE_INCLUDES(includeDirectoryString...);
    }
    else
    {
        return *this;
    }
}

// CppPrebuiltTarget
struct CppPT : public CPT
{
};

#endif // HMAKE_PREBUILT_HPP
