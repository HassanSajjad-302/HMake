
#ifndef HMAKE_GETTARGET_HPP
#define HMAKE_GETTARGET_HPP
#ifdef USE_HEADER_UNITS
import "CTargetRoundZeroBTarget.hpp";
import "Configuration.hpp";
import "DSC.hpp";
import "Cache.hpp";
#else
#include "CTargetRoundZeroBTarget.hpp"
#include "Cache.hpp"
#include "Configuration.hpp"
#include "DSC.hpp"
#endif

Configuration &GetConfiguration(const string &name);
Configuration &GetConfiguration(const string &name, CTarget &other, bool hasFile = true);

/*
CSourceTarget &GetCPreprocess(const string &name);
CSourceTarget &GetCPreprocess(const string &name, CTarget &other, bool hasFile = true);
CSourceTarget &GetCObject(const string &name);
CSourceTarget &GetCObject(const string &name, CTarget &other, bool hasFile = true);
*/

CppSourceTarget &GetCppPreprocess(const string &name);
CppSourceTarget &GetCppPreprocess(const string &name, CTarget &other, bool hasFile = true);
CppSourceTarget &GetCppObject(const string &name);
CppSourceTarget &GetCppObject(const string &name, CTarget &other, bool hasFile = true);

LinkOrArchiveTarget &GetExe(const string &name);
LinkOrArchiveTarget &GetExe(const string &name, CTarget &other, bool hasFile = true);
LinkOrArchiveTarget &GetStaticLinkOrArchiveTarget(const string &name);
LinkOrArchiveTarget &GetStaticLinkOrArchiveTarget(const string &name, CTarget &other, bool hasFile = true);
LinkOrArchiveTarget &GetSharedLinkOrArchiveTarget(const string &name);
LinkOrArchiveTarget &GetSharedLinkOrArchiveTarget(const string &name, CTarget &other, bool hasFile = true);

PrebuiltLinkOrArchiveTarget &GetPrebuiltLinkOrArchiveTarget(const string &name, const string &directory,
                                                            TargetType linkTargetType_);
PrebuiltLinkOrArchiveTarget &GetStaticPrebuiltLinkOrArchiveTarget(const string &name, const string &directory);
PrebuiltLinkOrArchiveTarget &GetSharedPrebuiltLinkOrArchiveTarget(const string &name, const string &directory);

// Cpp
DSC<CppSourceTarget> &GetCppExeDSC(const string &name, bool defines = false, string define = "");
DSC<CppSourceTarget> &GetCppExeDSC(const string &name, CTarget &other, bool defines = false, string define = "",
                                   bool hasFile = true);
DSC<CppSourceTarget> &GetCppTargetDSC(const string &name, bool defines = false, string define = "",
                                      TargetType targetType = cache.libraryType);
DSC<CppSourceTarget> &GetCppTargetDSC(const string &name, CTarget &other, bool defines = false, string define = "",
                                      TargetType targetType = cache.libraryType, bool hasFile = true);
DSC<CppSourceTarget> &GetCppDSC(const string &name, bool defines = false, string define = "",
                                TargetType targetType = cache.libraryType);
DSC<CppSourceTarget> &GetCppDSC(const string &name, CTarget &other, bool defines = false, string define = "",
                                TargetType targetType = cache.libraryType, bool hasFile = true);
DSC<CppSourceTarget> &GetCppStaticDSC(const string &name, bool defines = false, string define = "");
DSC<CppSourceTarget> &GetCppStaticDSC(const string &name, CTarget &other, bool defines = false, string define = "",
                                      bool hasFile = true);
DSC<CppSourceTarget> &GetCppSharedDSC(const string &name, bool defines = false, string define = "");
DSC<CppSourceTarget> &GetCppSharedDSC(const string &name, CTarget &other, bool defines = false, string define = "",
                                      bool hasFile = true);
DSC<CppSourceTarget> &GetCppObjectDSC(const string &name);
DSC<CppSourceTarget> &GetCppObjectDSC(const string &name, CTarget &other, bool hasFile = true);

// _P means that it will use PrebuiltLinkOrArchiveTarget instead of LinkOrArchiveTarget.
// GetCppObjectDSC_P and GetCppDSC_P are not provided because if PrebuiltLinkOrArchiveTarget * is nullptr, then the
// above functions can be used.

DSC<CppSourceTarget, true> &GetCppTargetDSC_P(const string &name, const string &directory, bool defines = false,
                                              string define = "", TargetType targetType = cache.libraryType);
DSC<CppSourceTarget, true> &GetCppTargetDSC_P(const string &name, const string &directory, CTarget &other,
                                              bool defines = false, string define = "",
                                              TargetType targetType = cache.libraryType, bool hasFile = true);
DSC<CppSourceTarget, true> &GetCppStaticDSC_P(const string &name, const string &directory, bool defines = false,
                                              string define = "");
DSC<CppSourceTarget, true> &GetCppStaticDSC_P(const string &name, const string &directory, CTarget &other,
                                              bool defines = false, string define = "", bool hasFile = true);
DSC<CppSourceTarget, true> &GetCppSharedDSC_P(const string &name, const string &directory, bool defines = false,
                                              string define = "");
DSC<CppSourceTarget, true> &GetCppSharedDSC_P(const string &name, const string &directory, CTarget &other,
                                              bool defines = false, string define = "", bool hasFile = true);

template <typename... U>
RoundZeroUpdateBTarget &GetRoundZeroUpdateBTarget(function<void(Builder &, unsigned short, BTarget &bTarget)> func,
                                                  U &...dependencies)
{
    RoundZeroUpdateBTarget &roundZeroUpdateBTarget = const_cast<RoundZeroUpdateBTarget &>(
        targets<RoundZeroUpdateBTarget>.emplace(std::move(func)).first.operator*());
    if constexpr (sizeof...(dependencies))
    {
        roundZeroUpdateBTarget.getRealBTarget(0).addDependency(dependencies...);
    }
    return roundZeroUpdateBTarget;
}

// TODO
// Also provide functions for GetCTargetRoundZeroBTarget here and in Configuration.

#endif // HMAKE_GETTARGET_HPP
