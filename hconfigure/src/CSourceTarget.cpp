
#ifdef USE_HEADER_UNITS
import "Prebuilt.hpp";
import "Features.hpp";
#else
#include "CSourceTarget.hpp"
#include "Features.hpp"
#endif

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