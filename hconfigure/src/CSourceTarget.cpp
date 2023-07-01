
#ifdef USE_HEADER_UNITS
import "Prebuilt.hpp";
import "Features.hpp";
#else
#include "CSourceTarget.hpp"
#include "Features.hpp"
#endif

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

bool operator<(const CSourceTarget &lhs, const CSourceTarget &rhs)
{
    return lhs.id < rhs.id;
}