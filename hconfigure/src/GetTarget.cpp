
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

Configuration &getConfiguration(const pstring &name)
{
    return const_cast<Configuration &>(targets<Configuration>.emplace(name).first.operator*());
}

Configuration &getConfiguration(bool buildExplicit, const pstring &name)
{
    return const_cast<Configuration &>(targets<Configuration>.emplace(name).first.operator*());
}

/*CSourceTarget &GetCPreprocess(const pstring &name)
{
    return const_cast<CSourceTarget &>(targets<CSourceTarget>.emplace(name, TargetType::PREPROCESS).first.operator*());
}

CSourceTarget &GetCPreprocess(bool buildExplicit, const pstring &name )
{
    return const_cast<CSourceTarget &>(
        targets<CSourceTarget>.emplace(name, TargetType::PREPROCESS ).first.operator*());
}

CSourceTarget &GetCObject(const pstring &name)
{
    return const_cast<CSourceTarget &>(
        targets<CSourceTarget>.emplace(name, TargetType::LIBRARY_OBJECT).first.operator*());
}

CSourceTarget &GetCObject(bool buildExplicit, const pstring &name )
{
    return const_cast<CSourceTarget &>(
        targets<CSourceTarget>.emplace(name, TargetType::LIBRARY_OBJECT ).first.operator*());
}*/

CppSourceTarget &getCppPreprocess(const pstring &name)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name, TargetType::PREPROCESS).first.operator*());
}

CppSourceTarget &getCppPreprocess(bool buildExplicit, const pstring &name)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(buildExplicit, name, TargetType::PREPROCESS).first.operator*());
}

CppSourceTarget &getCppObject(const pstring &name)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name, TargetType::LIBRARY_OBJECT).first.operator*());
}

CppSourceTarget &getCppObject(bool buildExplicit, const pstring &name)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(buildExplicit, name, TargetType::LIBRARY_OBJECT).first.operator*());
}

LinkOrArchiveTarget &GetExe(const pstring &name)
{
    return targets2<LinkOrArchiveTarget>.emplace_back(name, TargetType::EXECUTABLE);
}

LinkOrArchiveTarget &GetExe(bool buildExplicit, const pstring &name)
{
    return targets2<LinkOrArchiveTarget>.emplace_back(buildExplicit, name, TargetType::EXECUTABLE);
}

PrebuiltBasic &getPrebuiltBasic(const pstring &name_)
{
    return targets2<PrebuiltBasic>.emplace_back(name_, TargetType::LIBRARY_OBJECT);
}

LinkOrArchiveTarget &getStaticLinkOrArchiveTarget(const pstring &name)
{
    return targets2<LinkOrArchiveTarget>.emplace_back(name, TargetType::LIBRARY_STATIC);
}

LinkOrArchiveTarget &getStaticLinkOrArchiveTarget(bool buildExplicit, const pstring &name)
{
    return targets2<LinkOrArchiveTarget>.emplace_back(buildExplicit, name, TargetType::LIBRARY_STATIC);
}

LinkOrArchiveTarget &getSharedLinkOrArchiveTarget(const pstring &name)
{
    return targets2<LinkOrArchiveTarget>.emplace_back(name, TargetType::LIBRARY_SHARED);
}

LinkOrArchiveTarget &getSharedLinkOrArchiveTarget(bool buildExplicit, const pstring &name)
{
    return targets2<LinkOrArchiveTarget>.emplace_back(buildExplicit, name, TargetType::LIBRARY_SHARED);
}

PrebuiltLinkOrArchiveTarget &getPrebuiltLinkOrArchiveTarget(const pstring &name, const pstring &directory,
                                                            TargetType linkTargetType_)
{
    return targets2<PrebuiltLinkOrArchiveTarget>.emplace_back(name, directory, linkTargetType_);
}

PrebuiltLinkOrArchiveTarget &getStaticPrebuiltLinkOrArchiveTarget(const pstring &name, const pstring &directory)
{
    return targets2<PrebuiltLinkOrArchiveTarget>.emplace_back(name, directory, TargetType::LIBRARY_STATIC);
}

PrebuiltLinkOrArchiveTarget &getSharedPrebuiltLinkOrArchiveTarget(const pstring &name, const pstring &directory)
{
    return targets2<PrebuiltLinkOrArchiveTarget>.emplace_back(name, directory, TargetType::LIBRARY_SHARED);
}

/*CSourceTarget &GetCPT()
{
    CSourceTarget &cpt = const_cast<CSourceTarget &>(targets<CSourceTarget>.emplace().first.operator*());
    return cpt;
}*/

DSC<CppSourceTarget> &getCppExeDSC(const pstring &name, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(name + dashCpp),
                                                                                    &GetExe(name), defines,
                                              std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppExeDSC(bool buildExplicit, const pstring &name, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(name + dashCpp ),
                                              &GetExe(buildExplicit, name ), defines,
                                              std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppTargetDSC(const pstring &name, const TargetType targetType, const bool defines,
                                      pstring define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return getCppStaticDSC(name, defines, std::move(define));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return getCppSharedDSC(name, defines, std::move(define));
    }
    return getCppObjectDSC(name);
}

DSC<CppSourceTarget> &getCppTargetDSC(bool buildExplicit, const pstring &name, const TargetType targetType,
                                      const bool defines, pstring define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return getCppStaticDSC(buildExplicit, name, defines, std::move(define));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return getCppSharedDSC(buildExplicit, name, defines, std::move(define));
    }
    return getCppObjectDSC(buildExplicit, name);
}

DSC<CppSourceTarget> &getCppStaticDSC(const pstring &name, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(name + dashCpp), &getStaticLinkOrArchiveTarget(name),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppStaticDSC(bool buildExplicit, const pstring &name, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(buildExplicit, name + dashCpp ),
                                              &getStaticLinkOrArchiveTarget(buildExplicit, name ),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppSharedDSC(const pstring &name, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(name + dashCpp), &getSharedLinkOrArchiveTarget(name),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppSharedDSC(bool buildExplicit, const pstring &name, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(
                                         &getCppObject(buildExplicit, name + dashCpp ),
                                         &getSharedLinkOrArchiveTarget(buildExplicit, name ),
                                         defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppObjectDSC(const pstring &name, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(name + dashCpp), &getPrebuiltBasic(name), defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppObjectDSC(bool buildExplicit, const pstring &name, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(buildExplicit, name + dashCpp ), &getPrebuiltBasic(name), defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppTargetDSC_P(const pstring &name, const pstring &directory, const TargetType targetType,
                                        bool defines, pstring define)
{
    CppSourceTarget *cppSourceTarget = &getCppObject(name + dashCpp);
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &getStaticPrebuiltLinkOrArchiveTarget(name, directory), defines, std::move(define)).first.operator*());
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &getSharedPrebuiltLinkOrArchiveTarget(name, directory), defines, std::move(define)).first.operator*());
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
    throw std::exception{};
}

DSC<CppSourceTarget> &getCppTargetDSC_P(bool buildExplicit, const pstring &name, const pstring &directory,
                                        const TargetType targetType, bool defines, pstring define)
{
    CppSourceTarget *cppSourceTarget = &getCppObject(buildExplicit, name + dashCpp);
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &getStaticPrebuiltLinkOrArchiveTarget(name, directory), defines, std::move(define)).first.operator*());
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &getSharedPrebuiltLinkOrArchiveTarget(name, directory), defines, std::move(define)).first.operator*());
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
    throw std::exception{};
}

DSC<CppSourceTarget> &getCppTargetDSC_P(const pstring &name, const pstring &prebuiltName, const pstring &directory,
                                        const TargetType targetType, bool defines, pstring define)
{
    CppSourceTarget *cppSourceTarget = &getCppObject(name + dashCpp);
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &getStaticPrebuiltLinkOrArchiveTarget(prebuiltName, directory), defines, std::move(define)).first.operator*());
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &getSharedPrebuiltLinkOrArchiveTarget(prebuiltName, directory), defines, std::move(define)).first.operator*());
    }

    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
    throw std::exception{};
}

DSC<CppSourceTarget> &getCppTargetDSC_P(bool buildExplicit, const pstring &name, const pstring &prebuiltName,
                                        const pstring &directory, const TargetType targetType, bool defines,
                                        pstring define)
{
    CppSourceTarget *cppSourceTarget = &getCppObject(buildExplicit, name + dashCpp);
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &getStaticPrebuiltLinkOrArchiveTarget(prebuiltName, directory), defines, std::move(define)).first.operator*());
    }

    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &getSharedPrebuiltLinkOrArchiveTarget(prebuiltName, directory), defines, std::move(define)).first.operator*());
    }

    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
    throw std::exception{};
}

DSC<CppSourceTarget> &getCppStaticDSC_P(const pstring &name, const pstring &directory, const bool defines,
                                        pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(name + dashCpp), &getStaticPrebuiltLinkOrArchiveTarget(name, directory),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppStaticDSC_P(bool buildExplicit, const pstring &name, const pstring &directory,
                                        const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(buildExplicit, name + dashCpp ),
                                              &getStaticPrebuiltLinkOrArchiveTarget(name, directory),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppSharedDSC_P(const pstring &name, const pstring &directory, const bool defines,
                                        pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(name + dashCpp), &getSharedPrebuiltLinkOrArchiveTarget(name, directory),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppSharedDSC_P(bool buildExplicit, const pstring &name, const pstring &directory,
                                        const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(
                                         &getCppObject(buildExplicit, name + dashCpp ),
                                         &getSharedPrebuiltLinkOrArchiveTarget(name, directory),
                                         defines, std::move(define)).first.operator*());
}