
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

Configuration &GetConfiguration(const pstring &name);
Configuration &GetConfiguration(const pstring &name, CTarget &other, bool hasFile = true);

/*
CSourceTarget &GetCPreprocess(const pstring &name);
CSourceTarget &GetCPreprocess(const pstring &name, CTarget &other, bool hasFile = true);
CSourceTarget &GetCObject(const pstring &name);
CSourceTarget &GetCObject(const pstring &name, CTarget &other, bool hasFile = true);
*/

CppSourceTarget &GetCppPreprocess(const pstring &name);
CppSourceTarget &GetCppPreprocess(const pstring &name, CTarget &other, bool hasFile = true);
CppSourceTarget &GetCppObject(const pstring &name);
CppSourceTarget &GetCppObject(const pstring &name, CTarget &other, bool hasFile = true);

LinkOrArchiveTarget &GetExe(const pstring &name);
LinkOrArchiveTarget &GetExe(const pstring &name, CTarget &other, bool hasFile = true);
PrebuiltBasic &GetPrebuiltBasic(const pstring &name_);
LinkOrArchiveTarget &GetStaticLinkOrArchiveTarget(const pstring &name);
LinkOrArchiveTarget &GetStaticLinkOrArchiveTarget(const pstring &name, CTarget &other, bool hasFile = true);
LinkOrArchiveTarget &GetSharedLinkOrArchiveTarget(const pstring &name);
LinkOrArchiveTarget &GetSharedLinkOrArchiveTarget(const pstring &name, CTarget &other, bool hasFile = true);

PrebuiltLinkOrArchiveTarget &GetPrebuiltLinkOrArchiveTarget(const pstring &name, const pstring &directory,
                                                            TargetType linkTargetType_);
PrebuiltLinkOrArchiveTarget &GetStaticPrebuiltLinkOrArchiveTarget(const pstring &name, const pstring &directory);
PrebuiltLinkOrArchiveTarget &GetSharedPrebuiltLinkOrArchiveTarget(const pstring &name, const pstring &directory);

// Cpp
DSC<CppSourceTarget> &GetCppExeDSC(const pstring &name, bool defines = false, pstring define = "");
DSC<CppSourceTarget> &GetCppExeDSC(const pstring &name, CTarget &other, bool defines = false, pstring define = "",
                                   bool hasFile = true);
DSC<CppSourceTarget> &GetCppTargetDSC(const pstring &name, TargetType targetType = cache.libraryType,
                                      bool defines = false, pstring define = "");
DSC<CppSourceTarget> &GetCppTargetDSC(const pstring &name, CTarget &other, TargetType targetType = cache.libraryType,
                                      bool defines = false, pstring define = "", bool hasFile = true);
DSC<CppSourceTarget> &GetCppStaticDSC(const pstring &name, bool defines = false, pstring define = "");
DSC<CppSourceTarget> &GetCppStaticDSC(const pstring &name, CTarget &other, bool defines = false, pstring define = "",
                                      bool hasFile = true);
DSC<CppSourceTarget> &GetCppSharedDSC(const pstring &name, bool defines = false, pstring define = "");
DSC<CppSourceTarget> &GetCppSharedDSC(const pstring &name, CTarget &other, bool defines = false, pstring define = "",
                                      bool hasFile = true);
DSC<CppSourceTarget> &GetCppObjectDSC(const pstring &name, bool defines = false, pstring define = "");
DSC<CppSourceTarget> &GetCppObjectDSC(const pstring &name, CTarget &other, bool defines = false, pstring define = "",
                                      bool hasFile = true);

// _P means that it will use PrebuiltLinkOrArchiveTarget instead of LinkOrArchiveTarget.
// GetCppObjectDSC_P and GetCppDSC_P are not provided because if PrebuiltLinkOrArchiveTarget * is nullptr, then the
// above functions can be used.

DSC<CppSourceTarget> &GetCppTargetDSC_P(const pstring &name, const pstring &directory,
                                        TargetType targetType = cache.libraryType, bool defines = false,
                                        pstring define = "");
DSC<CppSourceTarget> &GetCppTargetDSC_P(const pstring &name, const pstring &directory, CTarget &other,
                                        TargetType targetType = cache.libraryType, bool defines = false,
                                        pstring define = "", bool hasFile = true);
DSC<CppSourceTarget> &GetCppTargetDSC_P(const pstring &name, const pstring &prebuiltName, const pstring &directory,
                                        TargetType targetType = cache.libraryType, bool defines = false,
                                        pstring define = "");
DSC<CppSourceTarget> &GetCppTargetDSC_P(const pstring &name, const pstring &prebuiltName, const pstring &directory,
                                        CTarget &other, TargetType targetType = cache.libraryType, bool defines = false,
                                        pstring define = "", bool hasFile = true);
DSC<CppSourceTarget> &GetCppStaticDSC_P(const pstring &name, const pstring &directory, bool defines = false,
                                        pstring define = "");
DSC<CppSourceTarget> &GetCppStaticDSC_P(const pstring &name, const pstring &directory, CTarget &other,
                                        bool defines = false, pstring define = "", bool hasFile = true);
DSC<CppSourceTarget> &GetCppSharedDSC_P(const pstring &name, const pstring &directory, bool defines = false,
                                        pstring define = "");
DSC<CppSourceTarget> &GetCppSharedDSC_P(const pstring &name, const pstring &directory, CTarget &other,
                                        bool defines = false, pstring define = "", bool hasFile = true);

template <typename... U>
RoundZeroUpdateBTarget &GetRoundZeroUpdateBTarget(function<void(class Builder &, BTarget &bTarget)> func,
                                                  U &...dependencies)
{
    RoundZeroUpdateBTarget &roundZeroUpdateBTarget = const_cast<RoundZeroUpdateBTarget &>(
        targets<RoundZeroUpdateBTarget>.emplace(std::move(func)).first.operator*());
    roundZeroUpdateBTarget.realBTargets[0].addDependency(dependencies...);
    return roundZeroUpdateBTarget;
}

// TODO
// Also provide functions for GetCTargetRoundZeroBTarget here and in Configuration.

#endif // HMAKE_GETTARGET_HPP
