
#ifdef USE_HEADER_UNITS
import "CSourceTarget.hpp";
import "Features.hpp";
#else
#include "CSourceTarget.hpp"
#include "Features.hpp"
#endif

void CSourceTarget::initializeCSourceTarget(const pstring &name_)
{
    const uint64_t index = pvalueIndexInSubArrayConsidered(tempCache, PValue(ptoref(name_)));

    if (bsMode == BSMode::CONFIGURE)
    {
        if (index == UINT64_MAX)
        {
            tempCache.PushBack(PValue(kArrayType), ralloc);
            targetTempCache = &tempCache[tempCache.Size() - 1];
            targetTempCache->PushBack(PValue(kStringType).SetString(name_.c_str(), name_.size(), ralloc), ralloc);
            targetTempCache->PushBack(PValue(kArrayType), ralloc);
            targetTempCache->PushBack(PValue(kArrayType), ralloc);
            tempCacheIndex = tempCache.Size() - 1;
        }
        else
        {
            targetTempCache = &tempCache[index];
            (*targetTempCache)[Indices::CppTarget::configCache].Clear();
        }
    }
    else
    {
        if (index != UINT64_MAX)
        {
            targetTempCache = &tempCache[index];
            tempCacheIndex = index;
        }
        else
        {
            printErrorMessage(fmt::format("Target {} not found in build-cache\n", name));
            exit(EXIT_FAILURE);
        }
    }
}

CSourceTarget::CSourceTarget(const pstring &name_) : ObjectFileProducerWithDS(name_, false, false)
{
    initializeCSourceTarget(name_);
}

CSourceTarget::CSourceTarget(const bool buildExplicit, const pstring &name_)
    : ObjectFileProducerWithDS(name_, buildExplicit, false)
{
    initializeCSourceTarget(name_);
}

CSourceTarget::CSourceTarget(const pstring &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, false, false), configuration(configuration_)
{
    initializeCSourceTarget(name_);
}

CSourceTarget::CSourceTarget(const bool buildExplicit, const pstring &name_, Configuration *configuration_)
    : ObjectFileProducerWithDS(name_, buildExplicit, false), configuration(configuration_)
{
    initializeCSourceTarget(name_);
}

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