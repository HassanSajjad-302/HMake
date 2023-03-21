
#ifndef HMAKE_PREBUILT_HPP
#define HMAKE_PREBUILT_HPP

#ifdef USE_HEADER_UNITS
import "ObjectFileProducer.hpp";
import <set>;
import <string>;
#else
#include "ObjectFileProducer.hpp"
#include <set>
#include <string>
#endif

using std::set, std::string;

// CPrebuiltTarget
struct CPT : public ObjectFileProducerWithDS<CPT>
{
    set<const class Node *> usageRequirementIncludes;
    string usageRequirementCompilerFlags;
    set<struct Define> usageRequirementCompileDefinitions;
};

// CppPrebuiltTarget
struct CppPT : public CPT
{
};

#endif // HMAKE_PREBUILT_HPP
