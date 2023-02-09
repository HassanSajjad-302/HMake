
#ifndef HMAKE_GETTARGET_HPP
#define HMAKE_GETTARGET_HPP

#include "CppSourceTarget.hpp"
#include "LinkOrArchiveTarget.hpp"

template <typename T> inline set<T> targets;

CppSourceTarget &GetPreProcessCpp(const string &name);
CppSourceTarget &GetCompileCpp(const string &name);
LinkOrArchiveTarget &GetExe(const string &name);
LinkOrArchiveTarget &GetLib(const string &name);
CppSourceTarget &GetExeCpp(const string &name);
LinkOrArchiveTarget &GetCppExe(const string &name);
CppSourceTarget &GetLibCpp(const string &name);
LinkOrArchiveTarget &GetCppLib(const string &name);

#endif // HMAKE_GETTARGET_HPP
