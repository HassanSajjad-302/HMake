
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
    if (bsMode == BSMode::CONFIGURE && useMiniTarget == UseMiniTarget::YES)
    {
        targetConfigCache = new PValue(kArrayType);
        targetConfigCache->PushBack(ptoref(name), ralloc);
        targetConfigCaches.emplace_back(targetConfigCache);
    }
    else
    {
        uint64_t index = pvalueIndexInSubArray(configCache, PValue(ptoref(name)));
        if (index != UINT64_MAX)
        {
            targetConfigCache = &configCache[index];
        }
        else
        {
            printErrorMessage(fmt::format("Target {} not found in config-cache\n", name));
            exit(EXIT_FAILURE);
        }
    }
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