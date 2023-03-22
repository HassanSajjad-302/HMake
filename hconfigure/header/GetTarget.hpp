
#ifndef HMAKE_GETTARGET_HPP
#define HMAKE_GETTARGET_HPP
#ifdef USE_HEADER_UNITS
import "DSC.hpp";
#else
#include "DSC.hpp"
#endif

CppSourceTarget &GetCppPreprocess(const string &name);
CppSourceTarget &GetCppPreprocess(const string &name, CTarget &other, bool hasFile = true);
CppSourceTarget &GetCppObject(const string &name);
CppSourceTarget &GetCppObject(const string &name, CTarget &other, bool hasFile = true);
LinkOrArchiveTarget &GetExe(const string &name);
LinkOrArchiveTarget &GetExe(const string &name, CTarget &other, bool hasFile = true);
LinkOrArchiveTarget &GetStatic(const string &name);
LinkOrArchiveTarget &GetStatic(const string &name, CTarget &other, bool hasFile = true);
LinkOrArchiveTarget &GetShared(const string &name);
LinkOrArchiveTarget &GetShared(const string &name, CTarget &other, bool hasFile = true);

PrebuiltLinkOrArchiveTarget &GetPrebuiltLinkOrArchiveTarget(const string &name, const string &directory,
                                                            TargetType linkTargetType_);
CPT &GetCPT();

DSC<CppSourceTarget> &GetCppExeDSC(const string &name);
DSC<CppSourceTarget> &GetCppExeDSC(const string &name, CTarget &other, bool hasFile = true);
DSC<CppSourceTarget> &GetCppDSC(const string &name);
DSC<CppSourceTarget> &GetCppDSC(const string &name, CTarget &other, bool hasFile = true);
DSC<CppSourceTarget> &GetCppStaticDSC(const string &name);
DSC<CppSourceTarget> &GetCppStaticDSC(const string &name, CTarget &other, bool hasFile = true);
DSC<CppSourceTarget> &GetCppSharedDSC(const string &name);
DSC<CppSourceTarget> &GetCppSharedDSC(const string &name, CTarget &other, bool hasFile = true);
DSC<CppSourceTarget> &GetCppObjectDSC(const string &name);
DSC<CppSourceTarget> &GetCppObjectDSC(const string &name, CTarget &other, bool hasFile = true);

DSCPrebuilt<CPT> &GetCPTDSC(const string &name, const string &directory, TargetType linkTargetType_);

#endif // HMAKE_GETTARGET_HPP
