
#include "GetTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Cache.hpp"

// TODO
//  Why _CPP_SOURCE is embedded with name?

CppSourceTarget &GetPreProcessCpp(const string &name)
{

    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::PREPROCESS).first.operator*());
}

CppSourceTarget &GetPreProcessCpp(const string &name, CTarget &other, bool hasFile)
{

    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::PREPROCESS, other, hasFile).first.operator*());
}

CppSourceTarget &GetCompileCpp(const string &name)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::COMPILE).first.operator*());
}

CppSourceTarget &GetCompileCpp(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::COMPILE, other, hasFile).first.operator*());
}

LinkOrArchiveTarget &GetExe(const string &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::EXECUTABLE).first.operator*());
}

LinkOrArchiveTarget &GetExe(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::EXECUTABLE, other, hasFile).first.operator*());
}

LinkOrArchiveTarget &GetLib(const string &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, cache.libraryType).first.operator*());
}

LinkOrArchiveTarget &GetLib(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, cache.libraryType, other, hasFile).first.operator*());
}

CppSourceTarget &GetExeCpp(const string &name)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = GetExe(name);
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::COMPILE, linkOrArchiveTarget, linkOrArchiveTarget).first.operator->());
    linkOrArchiveTarget.cppSourceTarget = cppSourceTarget;
    return *cppSourceTarget;
}

CppSourceTarget &GetExeCpp(const string &name, CTarget &other, bool hasFile)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = GetExe(name, other, hasFile);
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::COMPILE, linkOrArchiveTarget, linkOrArchiveTarget, hasFile).first.operator->());
    linkOrArchiveTarget.cppSourceTarget = cppSourceTarget;
    return *cppSourceTarget;
}

LinkOrArchiveTarget &GetCppExe(const string &name)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = GetExe(name);
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::COMPILE, linkOrArchiveTarget, linkOrArchiveTarget).first.operator->());
    linkOrArchiveTarget.cppSourceTarget = cppSourceTarget;
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &GetCppExe(const string &name, CTarget &other, bool hasFile)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = GetExe(name, other, hasFile);
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::COMPILE, linkOrArchiveTarget, linkOrArchiveTarget, hasFile).first.operator->());
    linkOrArchiveTarget.cppSourceTarget = cppSourceTarget;
    return linkOrArchiveTarget;
}

CppSourceTarget &GetLibCpp(const string &name)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = GetLib(name);
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::COMPILE, linkOrArchiveTarget, linkOrArchiveTarget).first.operator->());
    linkOrArchiveTarget.cppSourceTarget = cppSourceTarget;
    return *cppSourceTarget;
}

CppSourceTarget &GetLibCpp(const string &name, CTarget &other, bool hasFile)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = GetLib(name, other, hasFile);
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::COMPILE, linkOrArchiveTarget, linkOrArchiveTarget, hasFile).first.operator->());
    linkOrArchiveTarget.cppSourceTarget = cppSourceTarget;
    return *cppSourceTarget;
}

LinkOrArchiveTarget &GetCppLib(const string &name)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = GetLib(name);
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::COMPILE, linkOrArchiveTarget, linkOrArchiveTarget).first.operator->());
    linkOrArchiveTarget.cppSourceTarget = cppSourceTarget;
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &GetCppLib(const string &name, CTarget &other, bool hasFile)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = GetLib(name, other, hasFile);
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        targets<CppSourceTarget>.emplace(name + "-cpp", TargetType::COMPILE, linkOrArchiveTarget, linkOrArchiveTarget, hasFile).first.operator->());
    linkOrArchiveTarget.cppSourceTarget = cppSourceTarget;
    return linkOrArchiveTarget;
}