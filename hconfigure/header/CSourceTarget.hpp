
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

    vector<InclNode> reqIncls;
    vector<InclNode> useReqIncls;
    string reqCompilerFlags;
    string useReqCompilerFlags;
    flat_hash_set<Define> reqCompileDefinitions;
    flat_hash_set<Define> useReqCompileDefinitions;

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

    CSourceTarget &INTERFACE_COMPILER_FLAGS(const string &compilerFlags);
    CSourceTarget &INTERFACE_COMPILE_DEFINITION(const string &cddName, const string &cddValue = "");
    virtual CSourceTargetType getCSourceTargetType() const;
};
bool operator<(const CSourceTarget &lhs, const CSourceTarget &rhs);

#endif // HMAKE_CSOURCETARGET_HPP
