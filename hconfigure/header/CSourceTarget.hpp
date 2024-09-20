
#ifndef HMAKE_CSOURCETARGET_HPP
#define HMAKE_CSOURCETARGET_HPP

#ifdef USE_HEADER_UNITS
impoert "Features.hpp";
import "ObjectFileProducer.hpp";
import "SpecialNodes.hpp";
import <set>;
#else
#include "Features.hpp"
#include "ObjectFileProducer.hpp"
#include "SpecialNodes.hpp"
#include <set>
#endif

using std::set;

enum class CSourceTargetType : unsigned short
{
    CSourceTarget = 1,
    CppSourceTarget = 2,
};

class CSourceTarget : public ObjectFileProducerWithDS<CSourceTarget>
{
  public:
    using BaseType = CSourceTarget;
    vector<InclNode> useReqIncls;
    pstring usageRequirementCompilerFlags;
    set<Define> usageRequirementCompileDefinitions;
    PValue *targetTempCache = nullptr;
    // TODO:
    // Could be 4 bytes instead
    uint64_t tempCacheIndex = UINT64_MAX;
    struct Configuration *configuration = nullptr;

    void initializeCSourceTarget(const pstring &name_);
    explicit CSourceTarget(const pstring& name_);
    CSourceTarget(bool buildExplicit, const pstring& name_);
    CSourceTarget(const pstring& name_, Configuration *configuration);
    CSourceTarget(bool buildExplicit, const pstring& name_, Configuration *configuration_);

  protected:
    // This parameter noTargetCacheInitialization is here so CSourceTarget does not call CSourceTarget and not call
    // initializeCSourceTarget, as targetConfigCache could potentailly be set by the derived class
    explicit CSourceTarget(pstring name_, bool noTargetCacheInitialization);
    CSourceTarget(bool buildExplicit, pstring name_, bool noTargetCacheInitialization);
    CSourceTarget(pstring name_, Configuration *configuration_, bool noTargetCacheInitialization);
    CSourceTarget(bool buildExplicit, pstring name_, Configuration *configuration_, bool noTargetCacheInitialization);

  public:
    template <typename... U> CSourceTarget &interfaceIncludes(const pstring &include, U... includeDirectoryPString);
    CSourceTarget &INTERFACE_COMPILER_FLAGS(const pstring &compilerFlags);
    CSourceTarget &INTERFACE_COMPILE_DEFINITION(const pstring &cddName, const pstring &cddValue = "");
    virtual CSourceTargetType getCSourceTargetType() const;
};
bool operator<(const CSourceTarget &lhs, const CSourceTarget &rhs);

template <typename... U>
CSourceTarget &CSourceTarget::interfaceIncludes(const pstring &include, U... includeDirectoryPString)
{
    if (bsMode == BSMode::BUILD && useMiniTarget == UseMiniTarget::YES)
    {
        // Initialized in CppSourceTarget round 2
    }
    else
    {
        CppCompilerFeatures::actuallyAddInclude(useReqIncls, include, false);
    }

    if constexpr (sizeof...(includeDirectoryPString))
    {
        return interfaceIncludes(includeDirectoryPString...);
    }
    else
    {
        return *this;
    }
}

#endif // HMAKE_CSOURCETARGET_HPP
