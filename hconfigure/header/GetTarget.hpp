
#ifndef HMAKE_GETTARGET_HPP
#define HMAKE_GETTARGET_HPP

#include "CppSourceTarget.hpp"
#include "LinkOrArchiveTarget.hpp"

CppSourceTarget &GetPreProcessCpp(const string &name);
CppSourceTarget &GetPreProcessCpp(const string &name, CTarget &other, bool hasFile = true);
CppSourceTarget &GetCompileCpp(const string &name);
CppSourceTarget &GetCompileCpp(const string &name, CTarget &other, bool hasFile = true);
LinkOrArchiveTarget &GetExe(const string &name);
LinkOrArchiveTarget &GetExe(const string &name, CTarget &other, bool hasFile = true);
LinkOrArchiveTarget &GetLib(const string &name);
LinkOrArchiveTarget &GetLib(const string &name, CTarget &other, bool hasFile = true);
CppSourceTarget &GetExeCpp(const string &name);
CppSourceTarget &GetExeCpp(const string &name, CTarget &other, bool hasFile = true);
LinkOrArchiveTarget &GetCppExe(const string &name);
LinkOrArchiveTarget &GetCppExe(const string &name, CTarget &other, bool hasFile = true);
CppSourceTarget &GetLibCpp(const string &name);
CppSourceTarget &GetLibCpp(const string &name, CTarget &other, bool hasFile = true);
LinkOrArchiveTarget &GetCppLib(const string &name);
LinkOrArchiveTarget &GetCppLib(const string &name, CTarget &other, bool hasFile = true);

#endif // HMAKE_GETTARGET_HPP
