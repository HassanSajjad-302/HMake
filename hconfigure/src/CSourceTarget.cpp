
#ifdef USE_HEADER_UNITS
import "CSourceTarget.hpp";
import "Features.hpp";
#else
#include "CSourceTarget.hpp"
#include "Features.hpp"
#endif

CSourceTarget::CSourceTarget(bool buildExplicit, pstring name_)
    : ObjectFileProducerWithDS(std::move(name_), buildExplicit, false)
{
}

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