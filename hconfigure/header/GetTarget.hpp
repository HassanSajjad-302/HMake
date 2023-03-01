
#ifndef HMAKE_GETTARGET_HPP
#define HMAKE_GETTARGET_HPP

#include "DSC.hpp"

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

#endif // HMAKE_GETTARGET_HPP
