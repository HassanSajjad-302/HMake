
#ifdef USE_HEADER_UNITS
import "Prebuilt.hpp";
import "Features.hpp";
#else
#include "Prebuilt.hpp"
#include "Features.hpp"
#endif

CPT &CPT::INTERFACE_COMPILER_FLAGS(const string &compilerFlags)
{
    usageRequirementCompilerFlags += compilerFlags;
    return *this;
}

CPT &CPT::INTERFACE_COMPILE_DEFINITION(const string &cddName, const string &cddValue)
{
    usageRequirementCompileDefinitions.emplace(cddName, cddValue);
    return *this;
}