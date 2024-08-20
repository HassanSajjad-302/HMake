
#ifndef HMAKE_GETTARGET_HPP
#define HMAKE_GETTARGET_HPP
#ifdef USE_HEADER_UNITS
import "CTargetRoundZeroBTarget.hpp";
import "Configuration.hpp";
import "Cache.hpp";
#else
#include "CTargetRoundZeroBTarget.hpp"
#include "Cache.hpp"
#include "Configuration.hpp"
#endif

Configuration &getConfiguration(const pstring &name);

/*
CSourceTarget &GetCPreprocess(const pstring &name);
CSourceTarget &GetCPreprocess(bool buildExplicit, const pstring &name,  );
CSourceTarget &GetCObject(const pstring &name);
CSourceTarget &GetCObject(bool buildExplicit, const pstring &name,  );
*/

CppSourceTarget &getCppPreprocess(const pstring &name);
CppSourceTarget &getCppPreprocess(bool buildExplicit, const pstring &name);
CppSourceTarget &getCppObject(const pstring &name);
CppSourceTarget &getCppObject(bool buildExplicit, const pstring &name);

LinkOrArchiveTarget &GetExe(const pstring &name);
LinkOrArchiveTarget &GetExe(bool buildExplicit, const pstring &name);
PrebuiltBasic &getPrebuiltBasic(const pstring &name_);
LinkOrArchiveTarget &getStaticLinkOrArchiveTarget(const pstring &name);
LinkOrArchiveTarget &getStaticLinkOrArchiveTarget(bool buildExplicit, const pstring &name);
LinkOrArchiveTarget &getSharedLinkOrArchiveTarget(const pstring &name);
LinkOrArchiveTarget &getSharedLinkOrArchiveTarget(bool buildExplicit, const pstring &name);

PrebuiltLinkOrArchiveTarget &getPrebuiltLinkOrArchiveTarget(const pstring &name, const pstring &directory,
                                                            TargetType linkTargetType_);
PrebuiltLinkOrArchiveTarget &getStaticPrebuiltLinkOrArchiveTarget(const pstring &name, const pstring &directory);
PrebuiltLinkOrArchiveTarget &getSharedPrebuiltLinkOrArchiveTarget(const pstring &name, const pstring &directory);

// Cpp
DSC<CppSourceTarget> &getCppExeDSC(const pstring &name, bool defines = false, pstring define = "");
DSC<CppSourceTarget> &getCppExeDSC(bool buildExplicit, const pstring &name, bool defines = false, pstring define = "");
DSC<CppSourceTarget> &getCppTargetDSC(const pstring &name, TargetType targetType = cache.libraryType,
                                      bool defines = false, pstring define = "");
DSC<CppSourceTarget> &getCppTargetDSC(bool buildExplicit, const pstring &name,
                                      TargetType targetType = cache.libraryType, bool defines = false,
                                      pstring define = "");
DSC<CppSourceTarget> &getCppStaticDSC(const pstring &name, bool defines = false, pstring define = "");
DSC<CppSourceTarget> &getCppStaticDSC(bool buildExplicit, const pstring &name, bool defines = false,
                                      pstring define = "");
DSC<CppSourceTarget> &getCppSharedDSC(const pstring &name, bool defines = false, pstring define = "");
DSC<CppSourceTarget> &getCppSharedDSC(bool buildExplicit, const pstring &name, bool defines = false,
                                      pstring define = "");
DSC<CppSourceTarget> &getCppObjectDSC(const pstring &name, bool defines = false, pstring define = "");
DSC<CppSourceTarget> &getCppObjectDSC(bool buildExplicit, const pstring &name, bool defines = false,
                                      pstring define = "");

// _P means that it will use PrebuiltLinkOrArchiveTarget instead of LinkOrArchiveTarget.
// getCppObjectDSC_P and getCppDSC_P are not provided because if PrebuiltLinkOrArchiveTarget * is nullptr, then the
// above functions can be used.

DSC<CppSourceTarget> &getCppTargetDSC_P(const pstring &name, const pstring &directory,
                                        TargetType targetType = cache.libraryType, bool defines = false,
                                        pstring define = "");
DSC<CppSourceTarget> &getCppTargetDSC_P(bool buildExplicit, const pstring &name, const pstring &directory,
                                        TargetType targetType = cache.libraryType, bool defines = false,
                                        pstring define = "");
DSC<CppSourceTarget> &getCppTargetDSC_P(const pstring &name, const pstring &prebuiltName, const pstring &directory,
                                        TargetType targetType = cache.libraryType, bool defines = false,
                                        pstring define = "");
DSC<CppSourceTarget> &getCppTargetDSC_P(bool buildExplicit, const pstring &name, const pstring &prebuiltName,
                                        const pstring &directory, TargetType targetType = cache.libraryType,
                                        bool defines = false, pstring define = "");
DSC<CppSourceTarget> &getCppStaticDSC_P(const pstring &name, const pstring &directory, bool defines = false,
                                        pstring define = "");
DSC<CppSourceTarget> &getCppStaticDSC_P(bool buildExplicit, const pstring &name, const pstring &directory,
                                        bool defines = false, pstring define = "");
DSC<CppSourceTarget> &getCppSharedDSC_P(const pstring &name, const pstring &directory, bool defines = false,
                                        pstring define = "");
DSC<CppSourceTarget> &getCppSharedDSC_P(bool buildExplicit, const pstring &name, const pstring &directory,
                                        bool defines = false, pstring define = "");

template <typename... U>
RoundZeroUpdateBTarget &getRoundZeroUpdateBTarget(function<void(Builder &, BTarget &bTarget)> func, U &...dependencies)
{
    RoundZeroUpdateBTarget &roundZeroUpdateBTarget = const_cast<RoundZeroUpdateBTarget &>(
        targets<RoundZeroUpdateBTarget>.emplace(std::move(func)).first.operator*());
    roundZeroUpdateBTarget.realBTargets[0].addDependency(dependencies...);
    return roundZeroUpdateBTarget;
}

// TODO
// Also provide functions for GetCTargetRoundZeroBTarget here and in Configuration.

#endif // HMAKE_GETTARGET_HPP
