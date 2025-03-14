
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

Configuration::Configuration(const string &name_) : BTarget(name_, false, false)
{
}

void Configuration::initialize()
{
    compilerFeatures.initialize(*this);
    compilerFlags = compilerFeatures.getCompilerFlags();
    if (!stdCppTarget)
    {
        stdCppTarget = &getCppObjectDSC("std");
        CppSourceTarget &cppTarget = stdCppTarget->getSourceTarget();
        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            cppTarget.reqIncls = cppTargetFeatures.reqIncls;
            cppTarget.initializeUseReqInclsFromReqIncls().initializeHuDirsFromReqIncls();
        }
        cppTarget.requirementCompileDefinitions = cppTargetFeatures.requirementCompileDefinitions;
        cppTarget.requirementCompilerFlags = cppTargetFeatures.requirementCompilerFlags;
    }
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

CppSourceTarget &Configuration::getCppObject(const string &name_)
{
    CppSourceTarget &cppSourceTarget = targets<CppSourceTarget>.emplace_back(name + slashc + name_, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return cppSourceTarget;
}

CppSourceTarget &Configuration::getCppObject(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                             const string &name_)
{
    CppSourceTarget &cppSourceTarget =
        targets<CppSourceTarget>.emplace_back(buildCacheFilesDirPath_, explicitBuild, name + slashc + name_, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return cppSourceTarget;
}

CppSourceTarget &Configuration::getCppObjectAddStdTarget(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                         const string &name_)
{
    CppSourceTarget &cppSourceTarget =
        targets<CppSourceTarget>.emplace_back(buildCacheFilesDirPath_, explicitBuild, name + slashc + name_, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return addStdCppDep(cppSourceTarget);
}

PrebuiltBasic &Configuration::getPrebuiltBasic(const string &name_) const
{
    PrebuiltBasic &prebuiltBasic =
        targets<PrebuiltBasic>.emplace_back(name + slashc + name_, TargetType::LIBRARY_OBJECT);
    static_cast<PrebuiltBasicFeatures &>(prebuiltBasic) = prebuiltBasicFeatures;
    return prebuiltBasic;
}

LinkOrArchiveTarget &Configuration::GetExeLinkOrArchiveTarget(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget =
        targets<LinkOrArchiveTarget>.emplace_back(name + slashc + name_, TargetType::EXECUTABLE);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::GetExeLinkOrArchiveTarget(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                              const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = targets<LinkOrArchiveTarget>.emplace_back(
        buildCacheFilesDirPath_, explicitBuild, name + slashc + name_, TargetType::EXECUTABLE);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::getStaticLinkOrArchiveTarget(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget =
        targets<LinkOrArchiveTarget>.emplace_back(name + slashc + name_, TargetType::LIBRARY_STATIC);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::getStaticLinkOrArchiveTarget(bool explicitBuild,
                                                                 const string &buildCacheFilesDirPath_,
                                                                 const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = targets<LinkOrArchiveTarget>.emplace_back(
        buildCacheFilesDirPath_, explicitBuild, name + slashc + name_, TargetType::LIBRARY_STATIC);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::getSharedLinkOrArchiveTarget(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget =
        targets<LinkOrArchiveTarget>.emplace_back(name + slashc + name_, TargetType::LIBRARY_SHARED);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::getSharedLinkOrArchiveTarget(bool explicitBuild,
                                                                 const string &buildCacheFilesDirPath_,
                                                                 const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = targets<LinkOrArchiveTarget>.emplace_back(
        buildCacheFilesDirPath_, explicitBuild, name + slashc + name_, TargetType::LIBRARY_SHARED);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::getPrebuiltLinkOrArchiveTarget(const string &name_, const string &directory,
                                                                           TargetType linkTargetType_)
{
    PrebuiltLinkOrArchiveTarget &linkOrArchiveTarget =
        targets<PrebuiltLinkOrArchiveTarget>.emplace_back(name + slashc + name_, directory, linkTargetType_);
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    prebuiltLinkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::getStaticPrebuiltLinkOrArchiveTarget(const string &name_,
                                                                                 const string &directory)
{
    PrebuiltLinkOrArchiveTarget &linkOrArchiveTarget =
        targets<PrebuiltLinkOrArchiveTarget>.emplace_back(name + slashc + name_, directory, TargetType::LIBRARY_STATIC);
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    prebuiltLinkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::getSharedPrebuiltLinkOrArchiveTarget(const string &name_,
                                                                                 const string &directory)
{
    PrebuiltLinkOrArchiveTarget &linkOrArchiveTarget =
        targets<PrebuiltLinkOrArchiveTarget>.emplace_back(name + slashc + name_, directory, TargetType::LIBRARY_SHARED);
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

CppSourceTarget &Configuration::addStdCppDep(CppSourceTarget &target)
{
    if (evaluate(AssignStandardCppTarget::YES) && stdCppTarget)
    {
        target.privateDeps(&stdCppTarget->getSourceTarget());
    }
    return target;
}

DSC<CppSourceTarget> &Configuration::addStdDSCCppDep(DSC<CppSourceTarget> &target)
{
    if (evaluate(AssignStandardCppTarget::YES) && stdCppTarget)
    {
        target.privateLibraries(stdCppTarget);
    }
    return target;
}

DSC<CppSourceTarget> &Configuration::getCppExeDSC(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &GetExeLinkOrArchiveTarget(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppExeDSC(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                  const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &GetExeLinkOrArchiveTarget(explicitBuild, buildCacheFilesDirPath_, name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC(const string &name_, const bool defines, string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(getCppStaticDSC(name_, defines, std::move(define)));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(getCppSharedDSC(name_, defines, std::move(define)));
    }
    return addStdDSCCppDep(getCppObjectDSC(name_, defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC(const bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                     const string &name_, const bool defines, string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(
            getCppStaticDSC(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(
            getCppSharedDSC(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
    }
    return addStdDSCCppDep(getCppObjectDSC(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSC(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getStaticLinkOrArchiveTarget(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSC(const bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                     const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &getStaticLinkOrArchiveTarget(explicitBuild, buildCacheFilesDirPath_, name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSC(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getSharedLinkOrArchiveTarget(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSC(const bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                     const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &getSharedLinkOrArchiveTarget(explicitBuild, buildCacheFilesDirPath_, name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppObjectDSC(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getPrebuiltBasic(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppObjectDSC(const bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                     const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp), &getPrebuiltBasic(name_), defines,
        std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC_P(const string &name_, const string &directory, const bool defines,
                                                       string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(getCppStaticDSC_P(name_, directory, defines, define));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(getCppSharedDSC_P(name_, directory, defines, define));
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
    throw std::exception{};
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC_P(const string &name_, const string &prebuiltName,
                                                       const string &directory, bool defines, string define)
{
    CppSourceTarget *cppSourceTarget = &getCppObject(name_ + dashCpp);
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
            cppSourceTarget, &getStaticPrebuiltLinkOrArchiveTarget(prebuiltName, directory), defines));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
            cppSourceTarget, &getSharedPrebuiltLinkOrArchiveTarget(prebuiltName, directory), defines));
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
    throw std::exception{};
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSC_P(const string &name_, const string &directory, const bool defines,
                                                       string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getStaticPrebuiltLinkOrArchiveTarget(name_, directory), defines,
        std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSC_P(const string &name_, const string &directory, const bool defines,
                                                       string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObject(name_ + dashCpp), &getSharedPrebuiltLinkOrArchiveTarget(name_, directory), defines,
        std::move(define)));
}

CppSourceTarget &Configuration::getCppObjectNoName(const string &name_)
{
    CppSourceTarget &cppSourceTarget = targets<CppSourceTarget>.emplace_back(name_, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return cppSourceTarget;
}

CppSourceTarget &Configuration::getCppObjectNoName(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                   const string &name_)
{
    CppSourceTarget &cppSourceTarget =
        targets<CppSourceTarget>.emplace_back(buildCacheFilesDirPath_, explicitBuild, name_, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return cppSourceTarget;
}

CppSourceTarget &Configuration::getCppObjectNoNameAddStdTarget(bool explicitBuild,
                                                               const string &buildCacheFilesDirPath_,
                                                               const string &name_)
{
    CppSourceTarget &cppSourceTarget =
        targets<CppSourceTarget>.emplace_back(buildCacheFilesDirPath_, explicitBuild, name_, this);
    cppSourceTargets.emplace_back(&cppSourceTarget);
    return addStdCppDep(cppSourceTarget);
}

PrebuiltBasic &Configuration::getPrebuiltBasicNoName(const string &name_) const
{
    PrebuiltBasic &prebuiltBasic = targets<PrebuiltBasic>.emplace_back(name_, TargetType::LIBRARY_OBJECT);
    static_cast<PrebuiltBasicFeatures &>(prebuiltBasic) = prebuiltBasicFeatures;
    return prebuiltBasic;
}

LinkOrArchiveTarget &Configuration::GetExeLinkOrArchiveTargetNoName(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = targets<LinkOrArchiveTarget>.emplace_back(name_, TargetType::EXECUTABLE);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::GetExeLinkOrArchiveTargetNoName(bool explicitBuild,
                                                                    const string &buildCacheFilesDirPath_,
                                                                    const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = targets<LinkOrArchiveTarget>.emplace_back(
        buildCacheFilesDirPath_, explicitBuild, name_, TargetType::EXECUTABLE);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::getStaticLinkOrArchiveTargetNoName(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget =
        targets<LinkOrArchiveTarget>.emplace_back(name_, TargetType::LIBRARY_STATIC);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::getStaticLinkOrArchiveTargetNoName(bool explicitBuild,
                                                                       const string &buildCacheFilesDirPath_,
                                                                       const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = targets<LinkOrArchiveTarget>.emplace_back(
        buildCacheFilesDirPath_, explicitBuild, name_, TargetType::LIBRARY_STATIC);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::getSharedLinkOrArchiveTargetNoName(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget =
        targets<LinkOrArchiveTarget>.emplace_back(name_, TargetType::LIBRARY_SHARED);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::getSharedLinkOrArchiveTargetNoName(bool explicitBuild,
                                                                       const string &buildCacheFilesDirPath_,
                                                                       const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = targets<LinkOrArchiveTarget>.emplace_back(
        buildCacheFilesDirPath_, explicitBuild, name_, TargetType::LIBRARY_SHARED);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::getPrebuiltLinkOrArchiveTargetNoName(const string &name_,
                                                                                 const string &directory,
                                                                                 TargetType linkTargetType_)
{
    PrebuiltLinkOrArchiveTarget &linkOrArchiveTarget =
        targets<PrebuiltLinkOrArchiveTarget>.emplace_back(name_, directory, linkTargetType_);
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    prebuiltLinkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::getStaticPrebuiltLinkOrArchiveTargetNoName(const string &name_,
                                                                                       const string &directory)
{
    PrebuiltLinkOrArchiveTarget &linkOrArchiveTarget =
        targets<PrebuiltLinkOrArchiveTarget>.emplace_back(name_, directory, TargetType::LIBRARY_STATIC);
    static_cast<PrebuiltLinkerFeatures &>(linkOrArchiveTarget) = prebuiltLinkOrArchiveTargetFeatures;
    static_cast<PrebuiltBasicFeatures &>(linkOrArchiveTarget) = prebuiltBasicFeatures;
    prebuiltLinkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    return linkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::getSharedPrebuiltLinkOrArchiveTargetNoName(const string &name_,
                                                                                       const string &directory)
{
    PrebuiltLinkOrArchiveTarget &linkOrArchiveTarget =
        targets<PrebuiltLinkOrArchiveTarget>.emplace_back(name_, directory, TargetType::LIBRARY_SHARED);
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

DSC<CppSourceTarget> &Configuration::getCppExeDSCNoName(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &GetExeLinkOrArchiveTargetNoName(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppExeDSCNoName(bool explicitBuild, const string &buildCacheFilesDirPath_,
                                                        const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &GetExeLinkOrArchiveTargetNoName(explicitBuild, buildCacheFilesDirPath_, name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSCNoName(const string &name_, const bool defines, string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(getCppStaticDSCNoName(name_, defines, std::move(define)));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(getCppSharedDSCNoName(name_, defines, std::move(define)));
    }
    return addStdDSCCppDep(getCppObjectDSCNoName(name_, defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSCNoName(const bool explicitBuild,
                                                           const string &buildCacheFilesDirPath_, const string &name_,
                                                           const bool defines, string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(
            getCppStaticDSCNoName(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(
            getCppSharedDSCNoName(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
    }
    return addStdDSCCppDep(
        getCppObjectDSCNoName(explicitBuild, buildCacheFilesDirPath_, name_, defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSCNoName(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getStaticLinkOrArchiveTargetNoName(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSCNoName(const bool explicitBuild,
                                                           const string &buildCacheFilesDirPath_, const string &name_,
                                                           const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &getStaticLinkOrArchiveTargetNoName(explicitBuild, buildCacheFilesDirPath_, name_), defines,
        std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSCNoName(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getSharedLinkOrArchiveTargetNoName(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSCNoName(const bool explicitBuild,
                                                           const string &buildCacheFilesDirPath_, const string &name_,
                                                           const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp),
        &getSharedLinkOrArchiveTargetNoName(explicitBuild, buildCacheFilesDirPath_, name_), defines,
        std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppObjectDSCNoName(const string &name_, const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getPrebuiltBasicNoName(name_), defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppObjectDSCNoName(const bool explicitBuild,
                                                           const string &buildCacheFilesDirPath_, const string &name_,
                                                           const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(explicitBuild, buildCacheFilesDirPath_, name_ + dashCpp), &getPrebuiltBasicNoName(name_),
        defines, std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC_PNoName(const string &name_, const string &directory,
                                                             const bool defines, string define)
{
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(getCppStaticDSC_PNoName(name_, directory, defines, define));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(getCppSharedDSC_PNoName(name_, directory, defines, define));
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
    throw std::exception{};
}

DSC<CppSourceTarget> &Configuration::getCppTargetDSC_PNoName(const string &name_, const string &prebuiltName,
                                                             const string &directory, bool defines, string define)
{
    CppSourceTarget *cppSourceTarget = &getCppObjectNoName(name_ + dashCpp);
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
            cppSourceTarget, &getStaticPrebuiltLinkOrArchiveTargetNoName(prebuiltName, directory), defines));
    }
    if (targetType == TargetType::LIBRARY_SHARED)
    {
        return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
            cppSourceTarget, &getSharedPrebuiltLinkOrArchiveTargetNoName(prebuiltName, directory), defines));
    }
    printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
    throw std::exception{};
}

DSC<CppSourceTarget> &Configuration::getCppStaticDSC_PNoName(const string &name_, const string &directory,
                                                             const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getStaticPrebuiltLinkOrArchiveTargetNoName(name_, directory), defines,
        std::move(define)));
}

DSC<CppSourceTarget> &Configuration::getCppSharedDSC_PNoName(const string &name_, const string &directory,
                                                             const bool defines, string define)
{
    return addStdDSCCppDep(targets<DSC<CppSourceTarget>>.emplace_back(
        &getCppObjectNoName(name_ + dashCpp), &getSharedPrebuiltLinkOrArchiveTargetNoName(name_, directory), defines,
        std::move(define)));
}

bool operator<(const Configuration &lhs, const Configuration &rhs)
{
    return lhs.name < rhs.name;
}

Configuration &getConfiguration(const string &name)
{
    return targets<Configuration>.emplace_back(name);
}
