
#ifdef USE_HEADER_UNITS
import "CSourceTarget.hpp";
import "Features.hpp";
#else
#include "CSourceTarget.hpp"
#include "Features.hpp"
#endif

CSourceTarget::CSourceTarget(const string &name_) : ObjectFileProducerWithDS(name_, false, false), TargetCache(name_)
{
}

CSourceTarget::CSourceTarget(const bool buildExplicit, const string &name_)
    : ObjectFileProducerWithDS(name_, buildExplicit, false), TargetCache(name_)
{
}

CSourceTarget::CSourceTarget(const string &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, false, false), TargetCache(name_), configuration(configuration_)
{
}

CSourceTarget::CSourceTarget(const bool buildExplicit, const string &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, buildExplicit, false), TargetCache(name_), configuration(configuration_)
{
}

/*
CSourceTarget::CSourceTarget(string name_, const bool noTargetCacheInitialization)
    : ObjectFileProducerWithDS(std::move(name_), false, false)
{
}

CSourceTarget::CSourceTarget(const bool buildExplicit, string name_, const bool noTargetCacheInitialization)
    : ObjectFileProducerWithDS(std::move(name_), buildExplicit, false)
{
}

CSourceTarget::CSourceTarget(string name_, Configuration *configuration_, const bool noTargetCacheInitialization)
    : ObjectFileProducerWithDS(std::move(name_), false, false), configuration(configuration_)
{
}

CSourceTarget::CSourceTarget(const bool buildExplicit, string name_, Configuration *configuration_,
                             const bool noTargetCacheInitialization)
    : ObjectFileProducerWithDS(std::move(name_), buildExplicit, false), configuration(configuration_)
{
}

*/
CSourceTarget &CSourceTarget::INTERFACE_COMPILER_FLAGS(const string &compilerFlags)
{
    usageRequirementCompilerFlags += compilerFlags;
    return *this;
}

CSourceTarget &CSourceTarget::INTERFACE_COMPILE_DEFINITION(const string &cddName, const string &cddValue)
{
    usageRequirementCompileDefinitions.emplace(cddName, cddValue);
    return *this;
}

CSourceTargetType CSourceTarget::getCSourceTargetType() const
{
    return CSourceTargetType::CSourceTarget;
}

bool operator<(const CSourceTarget &lhs, const CSourceTarget &rhs)
{
    return lhs.id < rhs.id;
}