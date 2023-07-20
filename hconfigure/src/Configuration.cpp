
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

Configuration::Configuration(const pstring &name_) : CTarget{name_}
{
}

Configuration::Configuration(const pstring &name_, CTarget &other, bool hasFile) : CTarget{name_, other, hasFile}
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

CppSourceTarget &Configuration::GetCppPreprocess(const pstring &name_)
{
    CppSourceTarget &cppSourceTarget = const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>
            .emplace(name_ , TargetType::PREPROCESS, *this, configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator*());
    cppSourceTargets.emplace_back(&cppSourceTarget);
    static_cast<CppCompilerFeatures &>(cppSourceTarget) = compilerFeatures;
    if (moduleScope)
    {
        cppSourceTarget.setModuleScope(moduleScope);
    }
    return cppSourceTarget;
}

CppSourceTarget &Configuration::GetCppObject(const pstring &name_)
{
    CppSourceTarget &cppSourceTarget = const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>
            .emplace(name_, TargetType::LIBRARY_OBJECT, *this, configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator*());
    cppSourceTargets.emplace_back(&cppSourceTarget);
    static_cast<CppCompilerFeatures &>(cppSourceTarget) = compilerFeatures;
    if (moduleScope)
    {
        cppSourceTarget.setModuleScope(moduleScope);
    }
    return cppSourceTarget;
}

PrebuiltBasic &Configuration::GetPrebuiltBasic(const pstring &name_)
{
    return const_cast<PrebuiltBasic &>(targets<PrebuiltBasic>.emplace(name_).first.operator*());
}

LinkOrArchiveTarget &Configuration::GetExeLinkOrArchiveTarget(const pstring &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>
            .emplace(name_, TargetType::EXECUTABLE, *this, configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator*());
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::GetStaticLinkOrArchiveTarget(const pstring &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name_, TargetType::LIBRARY_STATIC, *this, configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator*());
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::GetSharedLinkOrArchiveTarget(const pstring &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name_, TargetType::LIBRARY_SHARED, *this, configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator*());
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    return linkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::GetPrebuiltLinkOrArchiveTarget(const pstring &name_,
                                                                           const pstring &directory,
                                                                           TargetType linkTargetType_)
{
    PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget = const_cast<PrebuiltLinkOrArchiveTarget &>(
        targets<PrebuiltLinkOrArchiveTarget>.emplace(name_, directory, linkTargetType_).first.operator*());
    prebuiltLinkOrArchiveTargets.emplace_back(&prebuiltLinkOrArchiveTarget);
    return prebuiltLinkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::GetStaticPrebuiltLinkOrArchiveTarget(const pstring &name_,
                                                                                 const pstring &directory)
{
    PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget = const_cast<PrebuiltLinkOrArchiveTarget &>(
        targets<PrebuiltLinkOrArchiveTarget>.emplace(name_, directory, TargetType::LIBRARY_STATIC).first.operator*());
    prebuiltLinkOrArchiveTargets.emplace_back(&prebuiltLinkOrArchiveTarget);
    return prebuiltLinkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::GetSharedPrebuiltLinkOrArchiveTarget(const pstring &name_,
                                                                                 const pstring &directory)
{
    PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget = const_cast<PrebuiltLinkOrArchiveTarget &>(
        targets<PrebuiltLinkOrArchiveTarget>.emplace(name_, directory, TargetType::LIBRARY_SHARED).first.operator*());
    prebuiltLinkOrArchiveTargets.emplace_back(&prebuiltLinkOrArchiveTarget);
    return prebuiltLinkOrArchiveTarget;
}

CSourceTarget &Configuration::GetCPT()
{
    CSourceTarget &cpt = const_cast<CSourceTarget &>(targets<CSourceTarget>.emplace().first.operator*());
    prebuiltTargets.emplace_back(&cpt);
    return cpt;
}

DSC<CppSourceTarget> &Configuration::GetCppExeDSC(const pstring &name_, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(
                                                                               &(GetCppObject(name_ + dashCpp)),
                                                                               &(GetExeLinkOrArchiveTarget(name_)), defines,
                                                                               std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppTargetDSC(const pstring &name_, TargetType targetType_, bool defines,
                                                     pstring define)
{
    if (targetType_ == TargetType::LIBRARY_STATIC)
    {
        return GetCppStaticDSC(name_, defines, std::move(define));
    }
    else if (targetType_ == TargetType::LIBRARY_SHARED)
    {
        return GetCppSharedDSC(name_, defines, std::move(define));
    }
    else
    {
        return GetCppObjectDSC(name_, defines, std::move(define));
    }
}

DSC<CppSourceTarget> &Configuration::GetCppStaticDSC(const pstring &name_, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name_ + dashCpp)),
                                                                                    &(GetStaticLinkOrArchiveTarget(name_)), defines,
                                                                                    std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppSharedDSC(const pstring &name_, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name_ + dashCpp)),
                                                                                    &(GetSharedLinkOrArchiveTarget(name_)), defines,
                                                                                    std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppObjectDSC(const pstring &name_, const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name_ + dashCpp)), &GetPrebuiltBasic(name_), defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppTargetDSC_P(const pstring &name_, const pstring &directory,
                                                       TargetType targetType_, bool defines, pstring define)
{
    if (targetType_ == TargetType::LIBRARY_STATIC)
    {
        return GetCppStaticDSC_P(name_, directory, defines, define);
    }
    else if (targetType_ == TargetType::LIBRARY_SHARED)
    {
        return GetCppSharedDSC_P(name_, directory, defines, define);
    }
    else
    {
        printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
        throw std::exception{};
    }
}

DSC<CppSourceTarget> &Configuration::GetCppTargetDSC_P(const pstring &name_, const pstring &prebuiltName,
                                                       const pstring &directory, TargetType targetType_, bool defines,
                                                       pstring define)
{
    CppSourceTarget *cppSourceTarget = &(GetCppObject(name_ + dashCpp));
    if (targetType_ == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &(GetStaticPrebuiltLinkOrArchiveTarget(prebuiltName, directory)), defines, std::move(define)).first.operator*());
    }
    else if (targetType_ == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget, &(GetSharedPrebuiltLinkOrArchiveTarget(prebuiltName, directory)), defines, std::move(define)).first.operator*());
    }
    else
    {
        printErrorMessage("TargetType should be one of TargetType::LIBRARY_STATIC or TargetType::LIBRARY_SHARED\n");
        throw std::exception{};
    }
}

DSC<CppSourceTarget> &Configuration::GetCppStaticDSC_P(const pstring &name_, const pstring &directory,
                                                       const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name_ + dashCpp)), &(GetStaticPrebuiltLinkOrArchiveTarget(name_, directory)),
                                                    defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppSharedDSC_P(const pstring &name_, const pstring &directory,
                                                       const bool defines, pstring define)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name_ + dashCpp)), &(GetSharedPrebuiltLinkOrArchiveTarget(name_, directory)),
                                                    defines, std::move(define)).first.operator*());
}

/*void Configuration::setJson()
{
    Json variantJson;
    variantJson[JConsts::configuration] = compilerFeatures.configurationType;
    variantJson[JConsts::compiler] = compilerFeatures.compiler;
    variantJson[JConsts::linker] = linkerFeatures.linker;
    variantJson[JConsts::archiver] = linkerFeatures.archiver;
    variantJson[JConsts::compilerFlags] = compilerFeatures.requirementCompilerFlags;
    variantJson[JConsts::compileDefinitions] = compilerFeatures.requirementCompileDefinitions;
    variantJson[JConsts::linkerFlags] = linkerFeatures.requirementLinkerFlags;
    variantJson[JConsts::libraryType] = linkerFeatures.libraryType;
    variantJson[JConsts::targets] = elements;
    json[0] = std::move(variantJson);
}*/

C_Target *Configuration::get_CAPITarget(BSMode bsModeLocal)
{
    auto *c_configuration = new C_Configuration();

    c_configuration->parent = reinterpret_cast<C_CTarget *>(CTarget::get_CAPITarget(bsModeLocal)->object);

    auto *c_Target = new C_Target();
    c_Target->type = C_TargetType::C_CONFIGURATION_TARGET_TYPE;
    c_Target->object = c_configuration;
    return c_Target;
}

bool operator<(const Configuration &lhs, const Configuration &rhs)
{
    return lhs.id < rhs.id;
}