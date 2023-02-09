
#include "GetTarget.hpp"
#include "Cache.hpp"

// TODO
//  Why _CPP_SOURCE is embedded with name?

CppSourceTarget &GetPreProcessCpp(const string &name)
{

    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::PREPROCESS).first.operator*());
}

CppSourceTarget &GetCompileCpp(const string &name)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::COMPILE).first.operator*());
}

LinkOrArchiveTarget &GetExe(const string &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::EXECUTABLE).first.operator*());
}

LinkOrArchiveTarget &GetLib(const string &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, cache.libraryType).first.operator*());
}

CppSourceTarget &GetExeCpp(const string &name)
{
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::COMPILE, GetExe(name)).first.operator->());
    cppSourceTarget->linkOrArchiveTarget->cppSourceTarget = cppSourceTarget;
    return *cppSourceTarget;
}

LinkOrArchiveTarget &GetCppExe(const string &name)
{
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::COMPILE, GetExe(name)).first.operator->());
    cppSourceTarget->linkOrArchiveTarget->cppSourceTarget = cppSourceTarget;
    return *(cppSourceTarget->linkOrArchiveTarget);
}

CppSourceTarget &GetLibCpp(const string &name)
{
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::COMPILE, GetLib(name)).first.operator->());
    cppSourceTarget->linkOrArchiveTarget->cppSourceTarget = cppSourceTarget;
    return *cppSourceTarget;
}

LinkOrArchiveTarget &GetCppLib(const string &name)
{
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::COMPILE, GetLib(name)).first.operator->());
    cppSourceTarget->linkOrArchiveTarget->cppSourceTarget = cppSourceTarget;
    return *(cppSourceTarget->linkOrArchiveTarget);
}