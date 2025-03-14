
#ifndef HMAKE_CSOURCETARGET_HPP
#define HMAKE_CSOURCETARGET_HPP

#ifdef USE_HEADER_UNITS
impoert "Features.hpp";
import "ObjectFileProducer.hpp";
import "SpecialNodes.hpp";
import "TargetCache.hpp";
#else
#include "Features.hpp"
#include "ObjectFileProducer.hpp"
#include "SpecialNodes.hpp"
#include "TargetCache.hpp"
#endif

enum class CSourceTargetType : unsigned short
{
    CSourceTarget = 1,
    CppSourceTarget = 2,
};

class CSourceTarget : public ObjectFileProducerWithDS<CSourceTarget>, public TargetCache
{
  public:
    using BaseType = CSourceTarget;
    vector<InclNode> useReqIncls;
    string usageRequirementCompilerFlags;
    flat_hash_set<Define> usageRequirementCompileDefinitions;
    Configuration *configuration = nullptr;

    explicit CSourceTarget(const string &name_);
    CSourceTarget(bool buildExplicit, const string &name_);
    CSourceTarget(const string &name_, Configuration *configuration);
    CSourceTarget(bool buildExplicit, const string &name_, Configuration *configuration_);

    /*protected:
      // This parameter noTargetCacheInitialization is here so CSourceTarget does not call CSourceTarget and not call
      // initializeCSourceTarget, as targetConfigCache could potentailly be set by the derived class
      explicit CSourceTarget(string name_, bool noTargetCacheInitialization);
      CSourceTarget(bool buildExplicit, string name_, bool noTargetCacheInitialization);
      CSourceTarget(string name_, Configuration *configuration_, bool noTargetCacheInitialization);
      CSourceTarget(bool buildExplicit, string name_, Configuration *configuration_, bool
      noTargetCacheInitialization);*/

  public:
    template <typename... U> CSourceTarget &interfaceIncludes(const string &include, U... includeDirectoryPString);
    CSourceTarget &INTERFACE_COMPILER_FLAGS(const string &compilerFlags);
    CSourceTarget &INTERFACE_COMPILE_DEFINITION(const string &cddName, const string &cddValue = "");
    virtual CSourceTargetType getCSourceTargetType() const;
};
bool operator<(const CSourceTarget &lhs, const CSourceTarget &rhs);

template <typename... U>
CSourceTarget &CSourceTarget::interfaceIncludes(const string &include, U... includeDirectoryPString)
{
    if constexpr (bsMode == BSMode::BUILD)
    {
        if (useMiniTarget == UseMiniTarget::YES)
        {
        }
    }
    else
    {
        // todo
        // CppCompilerFeatures::actuallyAddInclude(useReqIncls, include, false);
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
