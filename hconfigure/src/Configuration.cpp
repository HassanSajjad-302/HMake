
#ifdef USE_HEADER_UNITS
import "Configuration.hpp";
import "BuildSystemFunctions.hpp";
import "CppSourceTarget.hpp";
import "DSC.hpp";
import "LinkOrArchiveTarget.hpp";
#else
#include "Configuration.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include "DSC.hpp"
#include "LinkOrArchiveTarget.hpp"
#endif

Configuration::Configuration(const pstring &name_) : BTarget(name_, false, false)
{
}

void Configuration::markArchivePoint()
{
    // TODO
    // This functions marks the archive point i.e. the targets before this function should be archived upon successful
    // build. i.e. some extra info will be saved in build-cache.json file of these targets.
    // The goal is that next time when hbuild is invoked, archived targets source-files won't be checked for
    // existence/rebuilt. Neither the header-files
    // coming from such targets includes will be stored in cache.
    // The use-case is when e.g. a target A dependens on targets B and C, such that these targets source is never meant
    // to be changed e.g. fmt and json source in hmake project.
}

CppSourceTarget &Configuration::getCppPreprocess(const pstring &name_)
{
    CppSourceTarget &cppSourceTarget =
        targets2<CppSourceTarget>.emplace_back(name + slashc + name_, TargetType::PREPROCESS);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    static_cast<CppCompilerFeatures &>(cppSourceTarget) = compilerFeatures;
    return cppSourceTarget;
}

CppSourceTarget &Configuration::getCppObject(const pstring &name_)
{
    CppSourceTarget &cppSourceTarget =
        targets2<CppSourceTarget>.emplace_back(name + slashc + name_, TargetType::LIBRARY_OBJECT);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    static_cast<CppCompilerFeatures &>(cppSourceTarget) = compilerFeatures;
    return cppSourceTarget;
}

PrebuiltBasic &Configuration::getPrebuiltBasic(const pstring &name_) const
{
    PrebuiltBasic &prebuiltBasic =
        targets2<PrebuiltBasic>.emplace_back(name + slashc + name_, TargetType::LIBRARY_OBJECT);
    static_cast<PrebuiltBasicFeatures &>(prebuiltBasic) = prebuiltBasicFeatures;
    return prebuiltBasic;
}

LinkOrArchiveTarget &Configuration::GetExeLinkOrArchiveTarget(const pstring &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget =
        targets2<LinkOrArchiveTarget>.emplace_back(name + slashc + name_, TargetType::EXECUTABLE);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::getStaticLinkOrArchiveTarget(const pstring &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget =
        targets2<LinkOrArchiveTarget>.emplace_back(name + slashc + name_, TargetType::LIBRARY_STATIC);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::getSharedLinkOrArchiveTarget(const pstring &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget =
        targets2<LinkOrArchiveTarget>.emplace_back(name + slashc + name_, TargetType::LIBRARY_SHARED);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::getPrebuiltLinkOrArchiveTarget(const pstring &name_,
                                                                           const pstring &directory,
                                                                           TargetType linkTargetType_)
{
    PrebuiltLinkOrArchiveTarget &linkOrArchiveTarget =
        targets2<PrebuiltLinkOrArchiveTarget>.emplace_back(name + slashc + name_, directory, linkTargetType_);
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    prebuiltLinkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::getStaticPrebuiltLinkOrArchiveTarget(const pstring &name_,
                                                                                 const pstring &directory)
{
    PrebuiltLinkOrArchiveTarget &linkOrArchiveTarget = targets2<PrebuiltLinkOrArchiveTarget>.emplace_back(
        name + slashc + name_, directory, TargetType::LIBRARY_STATIC);
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    prebuiltLinkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::getSharedPrebuiltLinkOrArchiveTarget(const pstring &name_,
                                                                                 const pstring &directory)
{
    PrebuiltLinkOrArchiveTarget &linkOrArchiveTarget = targets2<PrebuiltLinkOrArchiveTarget>.emplace_back(
        name + slashc + name_, directory, TargetType::LIBRARY_SHARED);
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    prebuiltLinkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

/*
CSourceTarget &Configuration::GetCPT()
{
    CSourceTarget &cpt = const_cast<CSourceTarget &>(targets<CSourceTarget>.emplace().first.operator*());
    prebuiltTargets.emplace_back(&cpt);
    return cpt;
}
*/

DSC<CppSourceTarget> &Configuration::getCppExeDSC(const pstring &name_, const bool defines, pstring define)
{
    return targets2<DSC<CppSourceTarget>>.emplace_back(&getCppObject(name_ + dashCpp),
                                                       &GetExeLinkOrArchiveTarget(name_), defines, std::move(define));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC(const pstring &name_, const TargetType targetType_,
                                                     const bool defines, pstring define)
{
    if (targetType_ == TargetType::LIBRARY_STATIC)
    {
        return getCppStaticDSC(name_, defines, std::move(define));
    }
    if (targetType_ == TargetType::LIBRARY_SHARED)
    {
        return getCppSharedDSC(name_, defines, std::move(define));
    }
    return getCppObjectDSC(name_, defines, std::move(define));
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSC(const pstring &name_, const bool defines, pstring define)
{
    return targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getStaticLinkOrArchiveTarget(name_), defines, std::move(define));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSC(const pstring &name_, const bool defines, pstring define)
{
    return targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getSharedLinkOrArchiveTarget(name_), defines, std::move(define));
}

DSC<CppSourceTarget> &Configuration::getCppObjectDSC(const pstring &name_, const bool defines, pstring define)
{
    return targets2<DSC<CppSourceTarget>>.emplace_back(&getCppObject(name_ + dashCpp), &getPrebuiltBasic(name_),
                                                       defines, std::move(define));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC_P(const pstring &name_, const pstring &directory,
                                                       const TargetType targetType_, const bool defines, pstring define)
{
    if (targetType_ == TargetType::LIBRARY_STATIC)
    {
        return getCppStaticDSC_P(name_, directory, defines, define);
    }
    if (targetType_ == TargetType::LIBRARY_SHARED)
    {
        return getCppSharedDSC_P(name_, directory, defines, define);
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
    throw std::exception{};
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC_P(const pstring &name_, const pstring &prebuiltName,
                                                       const pstring &directory, const TargetType targetType_,
                                                       bool defines, pstring define)
{
    CppSourceTarget *cppSourceTarget = &getCppObject(name_ + dashCpp);
    if (targetType_ == TargetType::LIBRARY_STATIC)
    {
        return targets2<DSC<CppSourceTarget>>.emplace_back(
            cppSourceTarget, &getStaticPrebuiltLinkOrArchiveTarget(prebuiltName, directory), defines);
    }
    if (targetType_ == TargetType::LIBRARY_SHARED)
    {
        return targets2<DSC<CppSourceTarget>>.emplace_back(
            cppSourceTarget, &getSharedPrebuiltLinkOrArchiveTarget(prebuiltName, directory), defines);
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
    throw std::exception{};
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSC_P(const pstring &name_, const pstring &directory,
                                                       const bool defines, pstring define)
{
    return targets2<DSC<CppSourceTarget>>.emplace_back(&getCppObject(name_ + dashCpp),
                                                       &getStaticPrebuiltLinkOrArchiveTarget(name_, directory), defines,
                                                       std::move(define));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSC_P(const pstring &name_, const pstring &directory,
                                                       const bool defines, pstring define)
{
    return targets2<DSC<CppSourceTarget>>.emplace_back(&getCppObject(name_ + dashCpp),
                                                       &getSharedPrebuiltLinkOrArchiveTarget(name_, directory), defines,
                                                       std::move(define));
}

bool Configuration::getUseMiniTarget() const
{
    if (bsMode == BSMode::BUILD)
    {
        if (useMiniTarget == UseMiniTarget::YES)
        {
            return true;
        }
    }
    return false;
}

bool operator<(const Configuration &lhs, const Configuration &rhs)
{
    return lhs.name < rhs.name;
}