
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

Configuration &getConfiguration(const string &name)
{
    return const_cast<Configuration &>(targets<Configuration>.emplace(name).first.operator*());
}

Configuration &getConfiguration(bool buildExplicit, const string &name)
{
    return const_cast<Configuration &>(targets<Configuration>.emplace(name).first.operator*());
}

/*CSourceTarget &GetCPreprocess(const string &name)
{
    return const_cast<CSourceTarget &>(targets<CSourceTarget>.emplace(name, TargetType::PREPROCESS).first.operator*());
}

CSourceTarget &GetCPreprocess(bool buildExplicit, const string &name )
{
    return const_cast<CSourceTarget &>(
        targets<CSourceTarget>.emplace(name, TargetType::PREPROCESS ).first.operator*());
}

CSourceTarget &GetCObject(const string &name)
{
    return const_cast<CSourceTarget &>(
        targets<CSourceTarget>.emplace(name, TargetType::LIBRARY_OBJECT).first.operator*());
}

CSourceTarget &GetCObject(bool buildExplicit, const string &name )
{
    return const_cast<CSourceTarget &>(
        targets<CSourceTarget>.emplace(name, TargetType::LIBRARY_OBJECT ).first.operator*());
}*/

CppSourceTarget &getCppPreprocess(const string &name)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name, TargetType::PREPROCESS).first.operator*());
}

CppSourceTarget &getCppPreprocess(bool buildExplicit, const string &name)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(buildExplicit, name, TargetType::PREPROCESS).first.operator*());
}

CppSourceTarget &getCppObject(const string &name)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name, TargetType::LIBRARY_OBJECT).first.operator*());
}

CppSourceTarget &getCppObject(bool buildExplicit, const string &name)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(buildExplicit, name, TargetType::LIBRARY_OBJECT).first.operator*());
}

LinkOrArchiveTarget &GetExe(const string &name)
{
    return targets2<LinkOrArchiveTarget>.emplace_back(name, TargetType::EXECUTABLE);
}

LinkOrArchiveTarget &GetExe(bool buildExplicit, const string &name)
{
    return targets2<LinkOrArchiveTarget>.emplace_back(buildExplicit, name, TargetType::EXECUTABLE);
}

PrebuiltBasic &getPrebuiltBasic(const string &name_)
{
    return targets2<PrebuiltBasic>.emplace_back(name_, TargetType::LIBRARY_OBJECT);
}

LinkOrArchiveTarget &getStaticLinkOrArchiveTarget(const string &name)
{
    return targets2<LinkOrArchiveTarget>.emplace_back(name, TargetType::LIBRARY_STATIC);
}

LinkOrArchiveTarget &getStaticLinkOrArchiveTarget(bool buildExplicit, const string &name)
{
    return targets2<LinkOrArchiveTarget>.emplace_back(buildExplicit, name, TargetType::LIBRARY_STATIC);
}

LinkOrArchiveTarget &getSharedLinkOrArchiveTarget(const string &name)
{
    return targets2<LinkOrArchiveTarget>.emplace_back(name, TargetType::LIBRARY_SHARED);
}

LinkOrArchiveTarget &getSharedLinkOrArchiveTarget(bool buildExplicit, const string &name)
{
    return targets2<LinkOrArchiveTarget>.emplace_back(buildExplicit, name, TargetType::LIBRARY_SHARED);
}

PrebuiltLinkOrArchiveTarget &getPrebuiltLinkOrArchiveTarget(const string &name, const string &directory,
                                                            TargetType linkTargetType_)
{
    return targets2<PrebuiltLinkOrArchiveTarget>.emplace_back(name, directory, linkTargetType_);
}

PrebuiltLinkOrArchiveTarget &getStaticPrebuiltLinkOrArchiveTarget(const string &name, const string &directory)
{
    return targets2<PrebuiltLinkOrArchiveTarget>.emplace_back(name, directory, TargetType::LIBRARY_STATIC);
}

PrebuiltLinkOrArchiveTarget &getSharedPrebuiltLinkOrArchiveTarget(const string &name, const string &directory)
{
    return targets2<PrebuiltLinkOrArchiveTarget>.emplace_back(name, directory, TargetType::LIBRARY_SHARED);
}

/*CSourceTarget &GetCPT()
{
    CSourceTarget &cpt = const_cast<CSourceTarget &>(targets<CSourceTarget>.emplace().first.operator*());
    return cpt;
}*/

DSC<CppSourceTarget> &getCppExeDSC(const string &name, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(name + dashCpp),
                                                                                    &GetExe(name), defines,
                                              std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppExeDSC(bool buildExplicit, const string &name, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(name + dashCpp ),
                                              &GetExe(buildExplicit, name ), defines,
                                              std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppTargetDSC(const string &name, const TargetType targetType, const bool defines,
                                      string define)
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

DSC<CppSourceTarget> &getCppTargetDSC(bool buildExplicit, const string &name, const TargetType targetType,
                                      const bool defines, string define)
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

DSC<CppSourceTarget> &getCppStaticDSC(const string &name, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(name + dashCpp), &getStaticLinkOrArchiveTarget(name),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppStaticDSC(bool buildExplicit, const string &name, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(buildExplicit, name + dashCpp ),
                                              &getStaticLinkOrArchiveTarget(buildExplicit, name ),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppSharedDSC(const string &name, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(name + dashCpp), &getSharedLinkOrArchiveTarget(name),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppSharedDSC(bool buildExplicit, const string &name, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(
                                         &getCppObject(buildExplicit, name + dashCpp ),
                                         &getSharedLinkOrArchiveTarget(buildExplicit, name ),
                                         defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppObjectDSC(const string &name, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(name + dashCpp), &getPrebuiltBasic(name), defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppObjectDSC(bool buildExplicit, const string &name, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(buildExplicit, name + dashCpp ), &getPrebuiltBasic(name), defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppTargetDSC_P(const string &name, const string &directory, const TargetType targetType,
                                        bool defines, string define)
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

DSC<CppSourceTarget> &getCppTargetDSC_P(bool buildExplicit, const string &name, const string &directory,
                                        const TargetType targetType, bool defines, string define)
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

DSC<CppSourceTarget> &getCppTargetDSC_P(const string &name, const string &prebuiltName, const string &directory,
                                        const TargetType targetType, bool defines, string define)
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

DSC<CppSourceTarget> &getCppTargetDSC_P(bool buildExplicit, const string &name, const string &prebuiltName,
                                        const string &directory, const TargetType targetType, bool defines,
                                        string define)
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

DSC<CppSourceTarget> &getCppStaticDSC_P(const string &name, const string &directory, const bool defines,
                                        string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(name + dashCpp), &getStaticPrebuiltLinkOrArchiveTarget(name, directory),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppStaticDSC_P(bool buildExplicit, const string &name, const string &directory,
                                        const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(buildExplicit, name + dashCpp ),
                                              &getStaticPrebuiltLinkOrArchiveTarget(name, directory),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppSharedDSC_P(const string &name, const string &directory, const bool defines,
                                        string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&getCppObject(name + dashCpp), &getSharedPrebuiltLinkOrArchiveTarget(name, directory),
                                              defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &getCppSharedDSC_P(bool buildExplicit, const string &name, const string &directory,
                                        const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(
                                         &getCppObject(buildExplicit, name + dashCpp ),
                                         &getSharedPrebuiltLinkOrArchiveTarget(name, directory),
                                         defines, std::move(define)).first.operator*());
}