
#ifndef HMAKE_GETTARGET_HPP
#define HMAKE_GETTARGET_HPP

#include "CppSourceTarget.hpp"
#include "LinkOrArchiveTarget.hpp"

template <typename T> inline set<T> targets;

inline LinkOrArchiveTarget *makeCppExe(auto &&...args) // since C++20
{
    return targets<LinkOrArchiveTarget>.emplace(std::forward<decltype(args)>(args)...).first.operator->();
}

inline LinkOrArchiveTarget &GetExe(const string &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::EXECUTABLE).first.operator*());
}

inline LinkOrArchiveTarget &GetLib(const string &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, cache.libraryType).first.operator*());
}

inline LinkOrArchiveTarget &GetCppExe(const string &name)
{
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        targets<CppSourceTarget>.emplace(name + "_CPP_SOURCE", GetExe(name)).first.operator->());
    cppSourceTarget->linkOrArchiveTarget->cppSourceTarget = cppSourceTarget;
    return *(cppSourceTarget->linkOrArchiveTarget);
}

inline LinkOrArchiveTarget &GetCppLib(const string &name)
{
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        targets<CppSourceTarget>.emplace(name + "_CPP_SOURCE", GetLib(name)).first.operator->());
    cppSourceTarget->linkOrArchiveTarget->cppSourceTarget = cppSourceTarget;
    return *(cppSourceTarget->linkOrArchiveTarget);
}
#endif // HMAKE_GETTARGET_HPP
