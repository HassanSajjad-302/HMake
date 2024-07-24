
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

Configuration &GetConfiguration(const pstring &name)
{
    return const_cast<Configuration &>(targets<Configuration>.emplace(name).first.operator*());
}

Configuration &GetConfiguration(const pstring &name, CTarget &other, bool hasFile)
{
    return const_cast<Configuration &>(targets<Configuration>.emplace(name, other, hasFile).first.operator*());
}

/*CSourceTarget &GetCPreprocess(const pstring &name)
{
    return const_cast<CSourceTarget &>(targets<CSourceTarget>.emplace(name, TargetType::PREPROCESS).first.operator*());
}

CSourceTarget &GetCPreprocess(const pstring &name, CTarget &other, bool hasFile)
{
    return const_cast<CSourceTarget &>(
        targets<CSourceTarget>.emplace(name, TargetType::PREPROCESS, other, hasFile).first.operator*());
}

CSourceTarget &GetCObject(const pstring &name)
{
    return const_cast<CSourceTarget &>(
        targets<CSourceTarget>.emplace(name, TargetType::LIBRARY_OBJECT).first.operator*());
}

CSourceTarget &GetCObject(const pstring &name, CTarget &other, bool hasFile)
{
    return const_cast<CSourceTarget &>(
        targets<CSourceTarget>.emplace(name, TargetType::LIBRARY_OBJECT, other, hasFile).first.operator*());
}*/

CppSourceTarget &GetCppPreprocess(const pstring &name)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name, TargetType::PREPROCESS).first.operator*());
}

CppSourceTarget &GetCppPreprocess(const pstring &name, CTarget &other, bool hasFile)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name, TargetType::PREPROCESS, other, hasFile).first.operator*());
}

CppSourceTarget &GetCppObject(const pstring &name)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name, TargetType::LIBRARY_OBJECT).first.operator*());
}

CppSourceTarget &GetCppObject(const pstring &name, CTarget &other, bool hasFile)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name, TargetType::LIBRARY_OBJECT, other, hasFile).first.operator*());
}

LinkOrArchiveTarget &GetExe(const pstring &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::EXECUTABLE).first.operator*());
}

LinkOrArchiveTarget &GetExe(const pstring &name, CTarget &other, bool hasFile)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::EXECUTABLE, other, hasFile).first.operator*());
}

PrebuiltBasic &GetPrebuiltBasic(const pstring &name_)
{
    return const_cast<PrebuiltBasic &>(targets<PrebuiltBasic>.emplace(name_).first.operator*());
}

LinkOrArchiveTarget &GetStaticLinkOrArchiveTarget(const pstring &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::LIBRARY_STATIC).first.operator*());
}

LinkOrArchiveTarget &GetStaticLinkOrArchiveTarget(const pstring &name, CTarget &other, bool hasFile)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::LIBRARY_STATIC, other, hasFile).first.operator*());
}

LinkOrArchiveTarget &GetSharedLinkOrArchiveTarget(const pstring &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::LIBRARY_SHARED).first.operator*());
}

LinkOrArchiveTarget &GetSharedLinkOrArchiveTarget(const pstring &name, CTarget &other, bool hasFile)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::LIBRARY_SHARED, other, hasFile).first.operator*());
}

PrebuiltLinkOrArchiveTarget &GetPrebuiltLinkOrArchiveTarget(const pstring &name, const pstring &directory,
                                                            TargetType linkTargetType_)
{
    PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget = const_cast<PrebuiltLinkOrArchiveTarget &>(
        targets<PrebuiltLinkOrArchiveTarget>.emplace(name, directory, linkTargetType_).first.operator*());
    return prebuiltLinkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &GetStaticPrebuiltLinkOrArchiveTarget(const pstring &name, const pstring &directory)
{
    PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget = const_cast<PrebuiltLinkOrArchiveTarget &>(
        targets<PrebuiltLinkOrArchiveTarget>.emplace(name, directory, TargetType::LIBRARY_STATIC).first.operator*());
    return prebuiltLinkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &GetSharedPrebuiltLinkOrArchiveTarget(const pstring &name, const pstring &directory)
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

DSC<CppSourceTarget> &GetCppExeDSC(const pstring &name, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&GetCppObject(name + dashCpp),
                                                                                    &GetExe(name), defines,
                                              std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppExeDSC(const pstring &name, CTarget &other, const bool defines, pstring define,
                                   const bool hasFile)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&GetCppObject(name + dashCpp, other, hasFile),
                                              &GetExe(name, other, hasFile), defines,
                                              std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppTargetDSC(const pstring &name, const TargetType targetType, const bool defines, pstring define)
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

DSC<CppSourceTarget> &GetCppTargetDSC(const pstring &name, CTarget &other, const TargetType targetType,
                                      const bool defines,
                                      pstring define, const bool hasFile)
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

DSC<CppSourceTarget> &GetCppStaticDSC(const pstring &name, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&GetCppObject(name + dashCpp), &GetStaticLinkOrArchiveTarget(name),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppStaticDSC(const pstring &name, CTarget &other, const bool defines, pstring define,
                                      const bool hasFile)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&GetCppObject(name + dashCpp, other, hasFile),
                                              &GetStaticLinkOrArchiveTarget(name, other, hasFile),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppSharedDSC(const pstring &name, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&GetCppObject(name + dashCpp), &GetSharedLinkOrArchiveTarget(name),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppSharedDSC(const pstring &name, CTarget &other, const bool defines, pstring define,
                                      const bool hasFile)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(
                                         &GetCppObject(name + dashCpp, other, hasFile),
                                         &GetSharedLinkOrArchiveTarget(name, other, hasFile),
                                         defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppObjectDSC(const pstring &name, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&GetCppObject(name + dashCpp), &GetPrebuiltBasic(name), defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppObjectDSC(const pstring &name, CTarget &other, const bool defines, pstring define,
                                      const bool hasFile)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&GetCppObject(name + dashCpp, other, hasFile), &GetPrebuiltBasic(name), defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppTargetDSC_P(const pstring &name, const pstring &directory, const TargetType targetType,
                                        bool defines, pstring define)
{
    CppSourceTarget *cppSourceTarget = &GetCppObject(name + dashCpp);
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &GetStaticPrebuiltLinkOrArchiveTarget(name, directory), defines, std::move(define)).first.operator*());
    }
    else if (targetType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &GetSharedPrebuiltLinkOrArchiveTarget(name, directory), defines, std::move(define)).first.operator*());
    }
    else
    {
        printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
        throw std::exception{};
    }
}

DSC<CppSourceTarget> &GetCppTargetDSC_P(const pstring &name, const pstring &directory, CTarget &other,
                                        const TargetType targetType, bool defines, pstring define, const bool hasFile)
{
    CppSourceTarget *cppSourceTarget = &GetCppObject(name + dashCpp, other, hasFile);
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &GetStaticPrebuiltLinkOrArchiveTarget(name, directory), defines, std::move(define)).first.operator*());
    }
    else if (targetType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &GetSharedPrebuiltLinkOrArchiveTarget(name, directory), defines, std::move(define)).first.operator*());
    }
    else
    {
        printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
        throw std::exception{};
    }
}

DSC<CppSourceTarget> &GetCppTargetDSC_P(const pstring &name, const pstring &prebuiltName, const pstring &directory,
                                        const TargetType targetType, bool defines, pstring define)
{
    CppSourceTarget *cppSourceTarget = &GetCppObject(name + dashCpp);
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &GetStaticPrebuiltLinkOrArchiveTarget(prebuiltName, directory), defines, std::move(define)).first.operator*());
    }
    else if (targetType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &GetSharedPrebuiltLinkOrArchiveTarget(prebuiltName, directory), defines, std::move(define)).first.operator*());
    }
    else
    {
        printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
        throw std::exception{};
    }
}

DSC<CppSourceTarget> &GetCppTargetDSC_P(const pstring &name, const pstring &prebuiltName, const pstring &directory,
                                        CTarget &other, const TargetType targetType, bool defines, pstring define,
                                        const bool hasFile)
{
    CppSourceTarget *cppSourceTarget = &GetCppObject(name + dashCpp, other, hasFile);
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &GetStaticPrebuiltLinkOrArchiveTarget(prebuiltName, directory), defines, std::move(define)).first.operator*());
    }
    else if (targetType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &GetSharedPrebuiltLinkOrArchiveTarget(prebuiltName, directory), defines, std::move(define)).first.operator*());
    }
    else
    {
        printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
        throw std::exception{};
    }
}

DSC<CppSourceTarget> &GetCppStaticDSC_P(const pstring &name, const pstring &directory, const bool defines,
                                        pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&GetCppObject(name + dashCpp), &GetStaticPrebuiltLinkOrArchiveTarget(name, directory),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppStaticDSC_P(const pstring &name, const pstring &directory, CTarget &other,
                                        const bool defines, pstring define, const bool hasFile)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&GetCppObject(name + dashCpp, other, hasFile),
                                              &GetStaticPrebuiltLinkOrArchiveTarget(name, directory),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppSharedDSC_P(const pstring &name, const pstring &directory, const bool defines,
                                        pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&GetCppObject(name + dashCpp), &GetSharedPrebuiltLinkOrArchiveTarget(name, directory),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &GetCppSharedDSC_P(const pstring &name, const pstring &directory, CTarget &other,
                                        const bool defines, pstring define, const bool hasFile)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(
                                         &GetCppObject(name + dashCpp, other, hasFile),
                                         &GetSharedPrebuiltLinkOrArchiveTarget(name, directory),
                                         defines, std::move(define)).first.operator*());
}