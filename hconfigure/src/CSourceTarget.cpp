
#ifdef USE_HEADER_UNITS
import "CSourceTarget.hpp";
import "Features.hpp";
#else
#include "CSourceTarget.hpp"
#include "Features.hpp"
#endif

CSourceTarget::CSourceTarget(const pstring &name_) : ObjectFileProducerWithDS(name_, false, false), TargetCache(name_)
{
}

CSourceTarget::CSourceTarget(const bool buildExplicit, const pstring &name_)
    : ObjectFileProducerWithDS(name_, buildExplicit, false), TargetCache(name_)
{
}

CSourceTarget::CSourceTarget(const pstring &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, false, false), TargetCache(name_), configuration(configuration_)
{
}

CSourceTarget::CSourceTarget(const bool buildExplicit, const pstring &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, buildExplicit, false), TargetCache(name_), configuration(configuration_)
{
}

/*
CSourceTarget::CSourceTarget(pstring name_, const bool noTargetCacheInitialization)
    : ObjectFileProducerWithDS(std::move(name_), false, false)
{
}

CSourceTarget::CSourceTarget(const bool buildExplicit, pstring name_, const bool noTargetCacheInitialization)
    : ObjectFileProducerWithDS(std::move(name_), buildExplicit, false)
{
}

CSourceTarget::CSourceTarget(pstring name_, Configuration *configuration_, const bool noTargetCacheInitialization)
    : ObjectFileProducerWithDS(std::move(name_), false, false), configuration(configuration_)
{
}

CSourceTarget::CSourceTarget(const bool buildExplicit, pstring name_, Configuration *configuration_,
                             const bool noTargetCacheInitialization)
    : ObjectFileProducerWithDS(std::move(name_), buildExplicit, false), configuration(configuration_)
{
}

*/
CSourceTarget &CSourceTarget::INTERFACE_COMPILER_FLAGS(const pstring &compilerFlags)
{
    usageRequirementCompilerFlags += compilerFlags;
    return *this;
}

CSourceTarget &CSourceTarget::INTERFACE_COMPILE_DEFINITION(const pstring &cddName, const pstring &cddValue)
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