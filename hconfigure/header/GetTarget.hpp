
#ifndef HMAKE_GETTARGET_HPP
#define HMAKE_GETTARGET_HPP
#ifdef USE_HEADER_UNITS
import "Configuration.hpp";
import "DSC.hpp";
#else
#include "Configure.hpp"
#include "DSC.hpp"
#endif

Configuration &GetConfiguration(const string &name);
Configuration &GetConfiguration(const string &name, CTarget &other, bool hasFile = true);
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

DSC<CppSourceTarget> &GetCppExeDSC(const string &name, bool defines = false, string define = "");
DSC<CppSourceTarget> &GetCppExeDSC(const string &name, CTarget &other, bool defines = false, string define = "",
                                   bool hasFile = true);
DSC<CppSourceTarget> &GetCppDSC(const string &name, bool defines = false, string define = "");
DSC<CppSourceTarget> &GetCppDSC(const string &name, CTarget &other, bool defines = false, string define = "",
                                bool hasFile = true);
DSC<CppSourceTarget> &GetCppStaticDSC(const string &name, bool defines = false, string define = "");
DSC<CppSourceTarget> &GetCppStaticDSC(const string &name, CTarget &other, bool defines = false, string define = "",
                                      bool hasFile = true);
DSC<CppSourceTarget> &GetCppSharedDSC(const string &name, bool defines = false, string define = "");
DSC<CppSourceTarget> &GetCppSharedDSC(const string &name, CTarget &other, bool defines = false, string define = "",
                                      bool hasFile = true);
DSC<CppSourceTarget> &GetCppObjectDSC(const string &name);
DSC<CppSourceTarget> &GetCppObjectDSC(const string &name, CTarget &other, bool hasFile = true);

DSCPrebuilt<CPT> &GetCPTLibraryDSC(const string &name, const string &directory, TargetType linkTargetType_,
                                   const bool defines = false, string define = "");

#endif // HMAKE_GETTARGET_HPP
