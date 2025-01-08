
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
        targets2<CppSourceTarget>.emplace_back(name + slashc + name_, TargetType::PREPROCESS, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return cppSourceTarget;
}

CppSourceTarget &Configuration::getCppObject(const pstring &name_)
{
    CppSourceTarget &cppSourceTarget =
        targets2<CppSourceTarget>.emplace_back(name + slashc + name_, TargetType::LIBRARY_OBJECT, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return cppSourceTarget;
}

CppSourceTarget &Configuration::getCppObject(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                             const pstring &name_)
{
    CppSourceTarget &cppSourceTarget = targets2<CppSourceTarget>.emplace_back(
        buildCacheFilesDirPath_, explicitBuild, name + slashc + name_, TargetType::LIBRARY_OBJECT, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
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

LinkOrArchiveTarget &Configuration::GetExeLinkOrArchiveTarget(bool explicitBuild,
                                                              const pstring &buildCacheFilesDirPath_,
                                                              const pstring &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = targets2<LinkOrArchiveTarget>.emplace_back(
        buildCacheFilesDirPath_, explicitBuild, name + slashc + name_, TargetType::EXECUTABLE);
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

LinkOrArchiveTarget &Configuration::getStaticLinkOrArchiveTarget(bool explicitBuild,
                                                                 const pstring &buildCacheFilesDirPath_,
                                                                 const pstring &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = targets2<LinkOrArchiveTarget>.emplace_back(
        buildCacheFilesDirPath_, explicitBuild, name + slashc + name_, TargetType::LIBRARY_STATIC);
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

LinkOrArchiveTarget &Configuration::getSharedLinkOrArchiveTarget(bool explicitBuild,
                                                                 const pstring &buildCacheFilesDirPath_,
                                                                 const pstring &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = targets2<LinkOrArchiveTarget>.emplace_back(
        buildCacheFilesDirPath_, explicitBuild, name + slashc + name_, TargetType::LIBRARY_SHARED);
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

DSC<CppSourceTarget> &Configuration::addStdCppDep(DSC<CppSourceTarget> &target)
{
    if (evaluate(AssignStandardCppTarget::YES))
    {
        if (!stdCppTarget)
        {
            throw std::exception(
                fmt::format(
                    "Standard Cpp Target not is nullptr in configuration {}\n.Trying to add it as dep of target {}\n",
                    name, target.getSourceTarget().name)
                    .c_str());
        }
        target.privateLibraries(stdCppTarget);
    }
    return target;
}

DSC<CppSourceTarget> &Configuration::getCppExeDSC(const pstring &name_, const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &GetExeLinkOrArchiveTarget(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppExeDSC(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                  const pstring &name_, const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &GetExeLinkOrArchiveTarget(explicitBuild, buildCacheFilesDirPath_, name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC(const pstring &name_, const TargetType targetType_,
                                                     const bool defines, pstring define)
{
    if (targetType_ == TargetType::LIBRARY_STATIC)
    {
        return addStdCppDep(getCppStaticDSC(name_, defines, std::move(define)));
    }
    if (targetType_ == TargetType::LIBRARY_SHARED)
    {
        return addStdCppDep(getCppSharedDSC(name_, defines, std::move(define)));
    }
    return addStdCppDep(getCppObjectDSC(name_, defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC(const bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                     const pstring &name_, const TargetType targetType_,
                                                     const bool defines, pstring define)
{
    if (targetType_ == TargetType::LIBRARY_STATIC)
    {
        return addStdCppDep(getCppStaticDSC(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
    }
    if (targetType_ == TargetType::LIBRARY_SHARED)
    {
        return addStdCppDep(getCppSharedDSC(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
    }
    return addStdCppDep(getCppObjectDSC(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSC(const pstring &name_, const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getStaticLinkOrArchiveTarget(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSC(const bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                     const pstring &name_, const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &getStaticLinkOrArchiveTarget(explicitBuild, buildCacheFilesDirPath_, name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSC(const pstring &name_, const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getSharedLinkOrArchiveTarget(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSC(const bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                     const pstring &name_, const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &getSharedLinkOrArchiveTarget(explicitBuild, buildCacheFilesDirPath_, name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppObjectDSC(const pstring &name_, const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getPrebuiltBasic(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppObjectDSC(const bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                     const pstring &name_, const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp), &getPrebuiltBasic(name_), defines,
        std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC_P(const pstring &name_, const pstring &directory,
                                                       const TargetType targetType_, const bool defines, pstring define)
{
    if (targetType_ == TargetType::LIBRARY_STATIC)
    {
        return addStdCppDep(getCppStaticDSC_P(name_, directory, defines, define));
    }
    if (targetType_ == TargetType::LIBRARY_SHARED)
    {
        return addStdCppDep(getCppSharedDSC_P(name_, directory, defines, define));
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
        return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
            cppSourceTarget, &getStaticPrebuiltLinkOrArchiveTarget(prebuiltName, directory), defines));
    }
    if (targetType_ == TargetType::LIBRARY_SHARED)
    {
        return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
            cppSourceTarget, &getSharedPrebuiltLinkOrArchiveTarget(prebuiltName, directory), defines));
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
    throw std::exception{};
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSC_P(const pstring &name_, const pstring &directory,
                                                       const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getStaticPrebuiltLinkOrArchiveTarget(name_, directory), defines,
        std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSC_P(const pstring &name_, const pstring &directory,
                                                       const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getSharedPrebuiltLinkOrArchiveTarget(name_, directory), defines,
        std::move(define)));
}

CppSourceTarget &Configuration::getCppPreprocessNoName(const pstring &name_)
{
    CppSourceTarget &cppSourceTarget = targets2<CppSourceTarget>.emplace_back(name_, TargetType::PREPROCESS, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return cppSourceTarget;
}

CppSourceTarget &Configuration::getCppObjectNoName(const pstring &name_)
{
    CppSourceTarget &cppSourceTarget = targets2<CppSourceTarget>.emplace_back(name_, TargetType::LIBRARY_OBJECT, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return cppSourceTarget;
}

CppSourceTarget &Configuration::getCppObjectNoName(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                   const pstring &name_)
{
    CppSourceTarget &cppSourceTarget = targets2<CppSourceTarget>.emplace_back(buildCacheFilesDirPath_, explicitBuild,
                                                                              name_, TargetType::LIBRARY_OBJECT, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return cppSourceTarget;
}

PrebuiltBasic &Configuration::getPrebuiltBasicNoName(const pstring &name_) const
{
    PrebuiltBasic &prebuiltBasic = targets2<PrebuiltBasic>.emplace_back(name_, TargetType::LIBRARY_OBJECT);
    static_cast<PrebuiltBasicFeatures &>(prebuiltBasic) = prebuiltBasicFeatures;
    return prebuiltBasic;
}

LinkOrArchiveTarget &Configuration::GetExeLinkOrArchiveTargetNoName(const pstring &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget =
        targets2<LinkOrArchiveTarget>.emplace_back(name_, TargetType::EXECUTABLE);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::GetExeLinkOrArchiveTargetNoName(bool explicitBuild,
                                                                    const pstring &buildCacheFilesDirPath_,
                                                                    const pstring &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = targets2<LinkOrArchiveTarget>.emplace_back(
        buildCacheFilesDirPath_, explicitBuild, name_, TargetType::EXECUTABLE);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::getStaticLinkOrArchiveTargetNoName(const pstring &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget =
        targets2<LinkOrArchiveTarget>.emplace_back(name_, TargetType::LIBRARY_STATIC);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::getStaticLinkOrArchiveTargetNoName(bool explicitBuild,
                                                                       const pstring &buildCacheFilesDirPath_,
                                                                       const pstring &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = targets2<LinkOrArchiveTarget>.emplace_back(
        buildCacheFilesDirPath_, explicitBuild, name_, TargetType::LIBRARY_STATIC);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::getSharedLinkOrArchiveTargetNoName(const pstring &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget =
        targets2<LinkOrArchiveTarget>.emplace_back(name_, TargetType::LIBRARY_SHARED);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::getSharedLinkOrArchiveTargetNoName(bool explicitBuild,
                                                                       const pstring &buildCacheFilesDirPath_,
                                                                       const pstring &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = targets2<LinkOrArchiveTarget>.emplace_back(
        buildCacheFilesDirPath_, explicitBuild, name_, TargetType::LIBRARY_SHARED);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::getPrebuiltLinkOrArchiveTargetNoName(const pstring &name_,
                                                                                 const pstring &directory,
                                                                                 TargetType linkTargetType_)
{
    PrebuiltLinkOrArchiveTarget &linkOrArchiveTarget =
        targets2<PrebuiltLinkOrArchiveTarget>.emplace_back(name_, directory, linkTargetType_);
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    prebuiltLinkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::getStaticPrebuiltLinkOrArchiveTargetNoName(const pstring &name_,
                                                                                       const pstring &directory)
{
    PrebuiltLinkOrArchiveTarget &linkOrArchiveTarget =
        targets2<PrebuiltLinkOrArchiveTarget>.emplace_back(name_, directory, TargetType::LIBRARY_STATIC);
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    prebuiltLinkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::getSharedPrebuiltLinkOrArchiveTargetNoName(const pstring &name_,
                                                                                       const pstring &directory)
{
    PrebuiltLinkOrArchiveTarget &linkOrArchiveTarget =
        targets2<PrebuiltLinkOrArchiveTarget>.emplace_back(name_, directory, TargetType::LIBRARY_SHARED);
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

DSC<CppSourceTarget> &Configuration::getCppExeDSCNoName(const pstring &name_, const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &GetExeLinkOrArchiveTargetNoName(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppExeDSCNoName(bool explicitBuild, const pstring &buildCacheFilesDirPath_,
                                                        const pstring &name_, const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &GetExeLinkOrArchiveTargetNoName(explicitBuild, buildCacheFilesDirPath_, name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSCNoName(const pstring &name_, const TargetType targetType_,
                                                           const bool defines, pstring define)
{
    if (targetType_ == TargetType::LIBRARY_STATIC)
    {
        return addStdCppDep(getCppStaticDSCNoName(name_, defines, std::move(define)));
    }
    if (targetType_ == TargetType::LIBRARY_SHARED)
    {
        return addStdCppDep(getCppSharedDSCNoName(name_, defines, std::move(define)));
    }
    return addStdCppDep(getCppObjectDSCNoName(name_, defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSCNoName(const bool explicitBuild,
                                                           const pstring &buildCacheFilesDirPath_, const pstring &name_,
                                                           const TargetType targetType_, const bool defines,
                                                           pstring define)
{
    if (targetType_ == TargetType::LIBRARY_STATIC)
    {
        return addStdCppDep(
            getCppStaticDSCNoName(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
    }
    if (targetType_ == TargetType::LIBRARY_SHARED)
    {
        return addStdCppDep(
            getCppSharedDSCNoName(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
    }
    return addStdCppDep(
        getCppObjectDSCNoName(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSCNoName(const pstring &name_, const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getStaticLinkOrArchiveTargetNoName(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSCNoName(const bool explicitBuild,
                                                           const pstring &buildCacheFilesDirPath_, const pstring &name_,
                                                           const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &getStaticLinkOrArchiveTargetNoName(explicitBuild, buildCacheFilesDirPath_, name_), defines,
        std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSCNoName(const pstring &name_, const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getSharedLinkOrArchiveTargetNoName(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSCNoName(const bool explicitBuild,
                                                           const pstring &buildCacheFilesDirPath_, const pstring &name_,
                                                           const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &getSharedLinkOrArchiveTargetNoName(explicitBuild, buildCacheFilesDirPath_, name_), defines,
        std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppObjectDSCNoName(const pstring &name_, const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getPrebuiltBasicNoName(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppObjectDSCNoName(const bool explicitBuild,
                                                           const pstring &buildCacheFilesDirPath_, const pstring &name_,
                                                           const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp), &getPrebuiltBasicNoName(name_),
        defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC_PNoName(const pstring &name_, const pstring &directory,
                                                             const TargetType targetType_, const bool defines,
                                                             pstring define)
{
    if (targetType_ == TargetType::LIBRARY_STATIC)
    {
        return addStdCppDep(getCppStaticDSC_PNoName(name_, directory, defines, define));
    }
    if (targetType_ == TargetType::LIBRARY_SHARED)
    {
        return addStdCppDep(getCppSharedDSC_PNoName(name_, directory, defines, define));
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
    throw std::exception{};
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC_PNoName(const pstring &name_, const pstring &prebuiltName,
                                                             const pstring &directory, const TargetType targetType_,
                                                             bool defines, pstring define)
{
    CppSourceTarget *cppSourceTarget = &getCppObjectNoName(name_ + dashCpp);
    if (targetType_ == TargetType::LIBRARY_STATIC)
    {
        return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
            cppSourceTarget, &getStaticPrebuiltLinkOrArchiveTargetNoName(prebuiltName, directory), defines));
    }
    if (targetType_ == TargetType::LIBRARY_SHARED)
    {
        return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
            cppSourceTarget, &getSharedPrebuiltLinkOrArchiveTargetNoName(prebuiltName, directory), defines));
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
    throw std::exception{};
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSC_PNoName(const pstring &name_, const pstring &directory,
                                                             const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getStaticPrebuiltLinkOrArchiveTargetNoName(name_, directory), defines,
        std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSC_PNoName(const pstring &name_, const pstring &directory,
                                                             const bool defines, pstring define)
{
    return addStdCppDep(targets2<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getSharedPrebuiltLinkOrArchiveTargetNoName(name_, directory), defines,
        std::move(define)));
}

bool operator<(const Configuration &lhs, const Configuration &rhs)
{
    return lhs.name < rhs.name;
}