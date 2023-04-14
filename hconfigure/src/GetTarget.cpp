
#ifdef USE_HEADER_UNITS
import "GetTarget.hpp";
import "BuildSystemFunctions.hpp";
import "Cache.hpp";
import "CppSourceTarget.hpp";
#else
#include "GetTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Cache.hpp"
#include "CppSourceTarget.hpp"
#endif

Configuration &GetConfiguration(const string &name)
{
    return const_cast<Configuration &>(targets<Configuration>.emplace(name).first.operator*());
}

Configuration &GetConfiguration(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<Configuration &>(targets<Configuration>.emplace(name, other, hasFile).first.operator*());
}

CppSourceTarget &GetCppPreprocess(const string &name)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name, TargetType::PREPROCESS).first.operator*());
}

CppSourceTarget &GetCppPreprocess(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name, TargetType::PREPROCESS, other, hasFile).first.operator*());
}

CppSourceTarget &GetCppObject(const string &name)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name, TargetType::LIBRARY_OBJECT).first.operator*());
}

CppSourceTarget &GetCppObject(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name, TargetType::LIBRARY_OBJECT, other, hasFile).first.operator*());
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

LinkOrArchiveTarget &GetStatic(const string &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::LIBRARY_STATIC).first.operator*());
}

LinkOrArchiveTarget &GetStatic(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::LIBRARY_STATIC, other, hasFile).first.operator*());
}

LinkOrArchiveTarget &GetShared(const string &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::LIBRARY_SHARED).first.operator*());
}

LinkOrArchiveTarget &GetShared(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::LIBRARY_SHARED, other, hasFile).first.operator*());
}

PrebuiltLinkOrArchiveTarget &GetPrebuiltLinkOrArchiveTarget(const string &name, const string &directory,
                                                            TargetType linkTargetType_)
{
    PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget = const_cast<PrebuiltLinkOrArchiveTarget &>(
        targets<PrebuiltLinkOrArchiveTarget>.emplace(name, directory, linkTargetType_).first.operator*());
    return prebuiltLinkOrArchiveTarget;
}

CPT &GetCPT()
{
    CPT &cpt = const_cast<CPT &>(targets<CPT>.emplace().first.operator*());
    return cpt;
}

DSC<CppSourceTarget> &GetCppExeDSC(const string &name, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name + dashCpp)),
                                                                                    &(GetExe(name)), defines,
                                              std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppExeDSC(const string &name, CTarget &other, const bool defines, string define, bool hasFile)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name + dashCpp, other, hasFile)),
                                              &(GetExe(name, other, hasFile)), defines,
                                              std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppDSC(const string &name, const bool defines, string define)
{
    CppSourceTarget *cppSourceTarget = &(GetCppObject(name + dashCpp));
    if (cache.libraryType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &(GetStatic(name)), defines, std::move(define)).first.operator*());
    }
    else if (cache.libraryType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &(GetStatic(name)), defines, std::move(define)).first.operator*());
    }
}

DSC<CppSourceTarget> &GetCppDSC(const string &name, CTarget &other, const bool defines, string define, bool hasFile)
{
    CppSourceTarget *cppSourceTarget = &(GetCppObject(name + dashCpp, other, hasFile));
    if (cache.libraryType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &(GetStatic(name, other, hasFile)),
                                                  defines, std::move(define)).first.operator*());
    }
    else if (cache.libraryType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &(GetShared(name, other, hasFile)),
                                                  defines, std::move(define)).first.operator*());
    }
    else
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget).first.operator*());
    }
}

DSC<CppSourceTarget> &GetCppStaticDSC(const string &name, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name + dashCpp)), &(GetStatic(name)),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppStaticDSC(const string &name, CTarget &other, const bool defines, string define,
                                      bool hasFile)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name + dashCpp, other, hasFile)),
                                              &(GetStatic(name, other, hasFile)),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppSharedDSC(const string &name, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name + dashCpp)), &(GetShared(name)),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppSharedDSC(const string &name, CTarget &other, const bool defines, string define,
                                      bool hasFile)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(
                                         &(GetCppObject(name + dashCpp, other, hasFile)),
                                         &(GetShared(name, other, hasFile)),
                                         defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppObjectDSC(const string &name)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name + dashCpp))).first.operator*());
}

DSC<CppSourceTarget> &GetCppObjectDSC(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name + dashCpp, other, hasFile))).first.operator*());
}

DSCPrebuilt<CPT> &GetCPTLibraryDSC(const string &name, const string &directory, TargetType linkTargetType_,
                                   const bool defines, string define)
{
    return const_cast<DSCPrebuilt<CPT> &>(
        targets<DSCPrebuilt<CPT>>.emplace(
                                     &(GetCPT()), &(GetPrebuiltLinkOrArchiveTarget(name, directory, linkTargetType_)),
                                     defines, std::move(define)).first.operator*());
}
