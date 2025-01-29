
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

Configuration &getConfiguration(const string &name);

/*
CSourceTarget &GetCPreprocess(const string &name);
CSourceTarget &GetCPreprocess(bool buildExplicit, const string &name,  );
CSourceTarget &GetCObject(const string &name);
CSourceTarget &GetCObject(bool buildExplicit, const string &name,  );
*/

CppSourceTarget &getCppPreprocess(const string &name);
CppSourceTarget &getCppPreprocess(bool buildExplicit, const string &name);
CppSourceTarget &getCppObject(const string &name);
CppSourceTarget &getCppObject(bool buildExplicit, const string &name);

LinkOrArchiveTarget &GetExe(const string &name);
LinkOrArchiveTarget &GetExe(bool buildExplicit, const string &name);
PrebuiltBasic &getPrebuiltBasic(const string &name_);
LinkOrArchiveTarget &getStaticLinkOrArchiveTarget(const string &name);
LinkOrArchiveTarget &getStaticLinkOrArchiveTarget(bool buildExplicit, const string &name);
LinkOrArchiveTarget &getSharedLinkOrArchiveTarget(const string &name);
LinkOrArchiveTarget &getSharedLinkOrArchiveTarget(bool buildExplicit, const string &name);

PrebuiltLinkOrArchiveTarget &getPrebuiltLinkOrArchiveTarget(const string &name, const string &directory,
                                                            TargetType linkTargetType_);
PrebuiltLinkOrArchiveTarget &getStaticPrebuiltLinkOrArchiveTarget(const string &name, const string &directory);
PrebuiltLinkOrArchiveTarget &getSharedPrebuiltLinkOrArchiveTarget(const string &name, const string &directory);

// Cpp
DSC<CppSourceTarget> &getCppExeDSC(const string &name, bool defines = false, string define = "");
DSC<CppSourceTarget> &getCppExeDSC(bool buildExplicit, const string &name, bool defines = false, string define = "");
DSC<CppSourceTarget> &getCppTargetDSC(const string &name, TargetType targetType = cache.libraryType,
                                      bool defines = false, string define = "");
DSC<CppSourceTarget> &getCppTargetDSC(bool buildExplicit, const string &name,
                                      TargetType targetType = cache.libraryType, bool defines = false,
                                      string define = "");
DSC<CppSourceTarget> &getCppStaticDSC(const string &name, bool defines = false, string define = "");
DSC<CppSourceTarget> &getCppStaticDSC(bool buildExplicit, const string &name, bool defines = false,
                                      string define = "");
DSC<CppSourceTarget> &getCppSharedDSC(const string &name, bool defines = false, string define = "");
DSC<CppSourceTarget> &getCppSharedDSC(bool buildExplicit, const string &name, bool defines = false,
                                      string define = "");
DSC<CppSourceTarget> &getCppObjectDSC(const string &name, bool defines = false, string define = "");
DSC<CppSourceTarget> &getCppObjectDSC(bool buildExplicit, const string &name, bool defines = false,
                                      string define = "");

// _P means that it will use PrebuiltLinkOrArchiveTarget instead of LinkOrArchiveTarget.
// getCppObjectDSC_P and getCppDSC_P are not provided because if PrebuiltLinkOrArchiveTarget * is nullptr, then the
// above functions can be used.

DSC<CppSourceTarget> &getCppTargetDSC_P(const string &name, const string &directory,
                                        TargetType targetType = cache.libraryType, bool defines = false,
                                        string define = "");
DSC<CppSourceTarget> &getCppTargetDSC_P(bool buildExplicit, const string &name, const string &directory,
                                        TargetType targetType = cache.libraryType, bool defines = false,
                                        string define = "");
DSC<CppSourceTarget> &getCppTargetDSC_P(const string &name, const string &prebuiltName, const string &directory,
                                        TargetType targetType = cache.libraryType, bool defines = false,
                                        string define = "");
DSC<CppSourceTarget> &getCppTargetDSC_P(bool buildExplicit, const string &name, const string &prebuiltName,
                                        const string &directory, TargetType targetType = cache.libraryType,
                                        bool defines = false, string define = "");
DSC<CppSourceTarget> &getCppStaticDSC_P(const string &name, const string &directory, bool defines = false,
                                        string define = "");
DSC<CppSourceTarget> &getCppStaticDSC_P(bool buildExplicit, const string &name, const string &directory,
                                        bool defines = false, string define = "");
DSC<CppSourceTarget> &getCppSharedDSC_P(const string &name, const string &directory, bool defines = false,
                                        string define = "");
DSC<CppSourceTarget> &getCppSharedDSC_P(bool buildExplicit, const string &name, const string &directory,
                                        bool defines = false, string define = "");

template <typename... U>
RoundZeroUpdateBTarget &getRoundZeroUpdateBTarget(function<void(Builder &, BTarget &bTarget)> func, U &...dependencies)
{
    RoundZeroUpdateBTarget &roundZeroUpdateBTarget = const_cast<RoundZeroUpdateBTarget &>(
        targets<RoundZeroUpdateBTarget>.emplace(std::move(func)).first.operator*());
    roundZeroUpdateBTarget.realBTargets[0].addDependency(dependencies...);
    return roundZeroUpdateBTarget;
}

#endif // HMAKE_GETTARGET_HPP
