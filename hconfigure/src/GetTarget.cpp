
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

/*CSourceTarget &GetCPreprocess(const string &name)
{
    return const_cast<CSourceTarget &>(targets<CSourceTarget>.emplace(name, TargetType::PREPROCESS).first.operator*());
}

CSourceTarget &GetCPreprocess(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<CSourceTarget &>(
        targets<CSourceTarget>.emplace(name, TargetType::PREPROCESS, other, hasFile).first.operator*());
}

CSourceTarget &GetCObject(const string &name)
{
    return const_cast<CSourceTarget &>(
        targets<CSourceTarget>.emplace(name, TargetType::LIBRARY_OBJECT).first.operator*());
}

CSourceTarget &GetCObject(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<CSourceTarget &>(
        targets<CSourceTarget>.emplace(name, TargetType::LIBRARY_OBJECT, other, hasFile).first.operator*());
}*/

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

LinkOrArchiveTarget &GetStaticLinkOrArchiveTarget(const string &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::LIBRARY_STATIC).first.operator*());
}

LinkOrArchiveTarget &GetStaticLinkOrArchiveTarget(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::LIBRARY_STATIC, other, hasFile).first.operator*());
}

LinkOrArchiveTarget &GetSharedLinkOrArchiveTarget(const string &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::LIBRARY_SHARED).first.operator*());
}

LinkOrArchiveTarget &GetSharedLinkOrArchiveTarget(const string &name, CTarget &other, bool hasFile)
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

PrebuiltLinkOrArchiveTarget &GetStaticPrebuiltLinkOrArchiveTarget(const string &name, const string &directory)
{
    PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget = const_cast<PrebuiltLinkOrArchiveTarget &>(
        targets<PrebuiltLinkOrArchiveTarget>.emplace(name, directory, TargetType::LIBRARY_STATIC).first.operator*());
    return prebuiltLinkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &GetSharedPrebuiltLinkOrArchiveTarget(const string &name, const string &directory)
{
    PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget = const_cast<PrebuiltLinkOrArchiveTarget &>(
        targets<PrebuiltLinkOrArchiveTarget>.emplace(name, directory, TargetType::LIBRARY_SHARED).first.operator*());
    return prebuiltLinkOrArchiveTarget;
}

CSourceTarget &GetCPT()
{
    CSourceTarget &cpt = const_cast<CSourceTarget &>(targets<CSourceTarget>.emplace().first.operator*());
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

DSC<CppSourceTarget> &GetCppTargetDSC(const string &name, TargetType targetType, bool defines, string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return GetCppStaticDSC(name, defines, std::move(define));
    }
    else if (targetType == TargetType::LIBRARY_SHARED)
    {
        return GetCppSharedDSC(name, defines, std::move(define));
    }
    else
    {
        return GetCppObjectDSC(name);
    }
}

DSC<CppSourceTarget> &GetCppTargetDSC(const string &name, CTarget &other, TargetType targetType, bool defines,
                                      string define, bool hasFile)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return GetCppStaticDSC(name, other, defines, std::move(define), hasFile);
    }
    else if (targetType == TargetType::LIBRARY_SHARED)
    {
        return GetCppSharedDSC(name, other, defines, std::move(define), hasFile);
    }
    else
    {
        return GetCppObjectDSC(name, other, hasFile);
    }
}

DSC<CppSourceTarget> &GetCppStaticDSC(const string &name, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name + dashCpp)), &(GetStaticLinkOrArchiveTarget(name)),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppStaticDSC(const string &name, CTarget &other, const bool defines, string define,
                                      bool hasFile)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name + dashCpp, other, hasFile)),
                                              &(GetStaticLinkOrArchiveTarget(name, other, hasFile)),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppSharedDSC(const string &name, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name + dashCpp)), &(GetSharedLinkOrArchiveTarget(name)),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppSharedDSC(const string &name, CTarget &other, const bool defines, string define,
                                      bool hasFile)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(
                                         &(GetCppObject(name + dashCpp, other, hasFile)),
                                         &(GetSharedLinkOrArchiveTarget(name, other, hasFile)),
                                         defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppObjectDSC(const string &name, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name + dashCpp)), nullptr, defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppObjectDSC(const string &name, CTarget &other, const bool defines, string define,
                                      bool hasFile)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name + dashCpp, other, hasFile)), nullptr, defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget, true> &GetCppTargetDSC_P(const string &name, const string &directory, TargetType targetType,
                                              bool defines, string define)
{
    CppSourceTarget *cppSourceTarget = &(GetCppObject(name + dashCpp));
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget, true> &>(
            targets<DSC<CppSourceTarget, true>>.emplace(cppSourceTarget, &(GetStaticPrebuiltLinkOrArchiveTarget(name, directory)), defines, std::move(define)).first.operator*());
    }
    else if (targetType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget, true> &>(
            targets<DSC<CppSourceTarget, true>>.emplace(cppSourceTarget, &(GetSharedPrebuiltLinkOrArchiveTarget(name, directory)), defines, std::move(define)).first.operator*());
    }
    else
    {
        printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
        throw std::exception{};
    }
}

DSC<CppSourceTarget, true> &GetCppTargetDSC_P(const string &name, const string &directory, CTarget &other,
                                              TargetType targetType, bool defines, string define, bool hasFile)
{
    CppSourceTarget *cppSourceTarget = &(GetCppObject(name + dashCpp, other, hasFile));
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget, true> &>(
            targets<DSC<CppSourceTarget, true>>.emplace(cppSourceTarget, &(GetStaticPrebuiltLinkOrArchiveTarget(name, directory)), defines, std::move(define)).first.operator*());
    }
    else if (targetType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget, true> &>(
            targets<DSC<CppSourceTarget, true>>.emplace(cppSourceTarget, &(GetSharedPrebuiltLinkOrArchiveTarget(name, directory)), defines, std::move(define)).first.operator*());
    }
    else
    {
        printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
        throw std::exception{};
    }
}

DSC<CppSourceTarget, true> &GetCppTargetDSC_P(const string &name, const string &prebuiltName, const string &directory,
                                              TargetType targetType, bool defines, string define)
{
    CppSourceTarget *cppSourceTarget = &(GetCppObject(name + dashCpp));
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget, true> &>(
            targets<DSC<CppSourceTarget, true>>.emplace(cppSourceTarget, &(GetStaticPrebuiltLinkOrArchiveTarget(prebuiltName, directory)), defines, std::move(define)).first.operator*());
    }
    else if (targetType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget, true> &>(
            targets<DSC<CppSourceTarget, true>>.emplace(cppSourceTarget, &(GetSharedPrebuiltLinkOrArchiveTarget(prebuiltName, directory)), defines, std::move(define)).first.operator*());
    }
    else
    {
        printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
        throw std::exception{};
    }
}

DSC<CppSourceTarget, true> &GetCppTargetDSC_P(const string &name, const string &prebuiltName, const string &directory,
                                              CTarget &other, TargetType targetType, bool defines, string define,
                                              bool hasFile)
{
    CppSourceTarget *cppSourceTarget = &(GetCppObject(name + dashCpp, other, hasFile));
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget, true> &>(
            targets<DSC<CppSourceTarget, true>>.emplace(cppSourceTarget, &(GetStaticPrebuiltLinkOrArchiveTarget(prebuiltName, directory)), defines, std::move(define)).first.operator*());
    }
    else if (targetType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget, true> &>(
            targets<DSC<CppSourceTarget, true>>.emplace(cppSourceTarget, &(GetSharedPrebuiltLinkOrArchiveTarget(prebuiltName, directory)), defines, std::move(define)).first.operator*());
    }
    else
    {
        printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
        throw std::exception{};
    }
}

DSC<CppSourceTarget, true> &GetCppStaticDSC_P(const string &name, const string &directory, const bool defines,
                                              string define)
{
    return const_cast<DSC<CppSourceTarget, true> &>(
        targets<DSC<CppSourceTarget, true>>.emplace(&(GetCppObject(name + dashCpp)), &(GetStaticPrebuiltLinkOrArchiveTarget(name, directory)),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget, true> &GetCppStaticDSC_P(const string &name, const string &directory, CTarget &other,
                                              const bool defines, string define, bool hasFile)
{
    return const_cast<DSC<CppSourceTarget, true> &>(
        targets<DSC<CppSourceTarget, true>>.emplace(&(GetCppObject(name + dashCpp, other, hasFile)),
                                              &(GetStaticPrebuiltLinkOrArchiveTarget(name, directory)),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget, true> &GetCppSharedDSC_P(const string &name, const string &directory, const bool defines,
                                              string define)
{
    return const_cast<DSC<CppSourceTarget, true> &>(
        targets<DSC<CppSourceTarget, true>>.emplace(&(GetCppObject(name + dashCpp)), &(GetSharedPrebuiltLinkOrArchiveTarget(name, directory)),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget, true> &GetCppSharedDSC_P(const string &name, const string &directory, CTarget &other,
                                              const bool defines, string define, bool hasFile)
{
    return const_cast<DSC<CppSourceTarget, true> &>(
        targets<DSC<CppSourceTarget, true>>.emplace(
                                         &(GetCppObject(name + dashCpp, other, hasFile)),
                                         &(GetSharedPrebuiltLinkOrArchiveTarget(name, directory)),
                                         defines, std::move(define)).first.operator*());
}