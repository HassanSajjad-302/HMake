
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

Configuration::Configuration(const string &name_) : CTarget{name_}
{
}

Configuration::Configuration(const string &name_, CTarget &other, bool hasFile) : CTarget{name_, other, hasFile}
{
}

void Configuration::markArchivePoint()
{
    // TODO
    // This functions marks the archive point i.e. the targets before this function should be archived upon successful
    // build. i.e. some extra info will be saved in target-cache file of these targets.
    // The goal is that next time when hbuild is invoked, archived targets source-files won't be checked for
    // existence/rebuilt. Neither will smfiles be generated for such targets module-files. Neither the header-files
    // coming from such targets includes will be stored in cache.
    // The use-case is when e.g. a target A dependens on targets B and C, such that these targets source is never meant
    // to be changed e.g. fmt and json source in hmake project.
}

CppSourceTarget &Configuration::GetCppPreprocess(const string &name_)
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

CppSourceTarget &Configuration::GetCppObject(const string &name_)
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

LinkOrArchiveTarget &Configuration::GetExeLinkOrArchiveTarget(const string &name_)
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

LinkOrArchiveTarget &Configuration::GetStaticLinkOrArchiveTarget(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name_, TargetType::LIBRARY_STATIC, *this, configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator*());
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::GetSharedLinkOrArchiveTarget(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name_, TargetType::LIBRARY_SHARED, *this, configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator*());
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    return linkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::GetPrebuiltLinkOrArchiveTarget(const string &name_, const string &directory,
                                                                           TargetType linkTargetType_)
{
    PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget = const_cast<PrebuiltLinkOrArchiveTarget &>(
        targets<PrebuiltLinkOrArchiveTarget>.emplace(name_, directory, linkTargetType_).first.operator*());
    prebuiltLinkOrArchiveTargets.emplace_back(&prebuiltLinkOrArchiveTarget);
    return prebuiltLinkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::GetStaticPrebuiltLinkOrArchiveTarget(const string &name_,
                                                                                 const string &directory)
{
    PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget = const_cast<PrebuiltLinkOrArchiveTarget &>(
        targets<PrebuiltLinkOrArchiveTarget>.emplace(name_, directory, TargetType::LIBRARY_STATIC).first.operator*());
    prebuiltLinkOrArchiveTargets.emplace_back(&prebuiltLinkOrArchiveTarget);
    return prebuiltLinkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::GetSharedPrebuiltLinkOrArchiveTarget(const string &name_,
                                                                                 const string &directory)
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

DSC<CppSourceTarget> &Configuration::GetCppDSC(const string &name_, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(
                                                                               &(GetCppObject(name_ + dashCpp)),
                                                                               &(GetExeLinkOrArchiveTarget(name_)), defines,
                                                                               std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppExeDSC(const string &name_, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(
                                                                               &(GetCppObject(name_ + dashCpp)),
                                                                               &(GetExeLinkOrArchiveTarget(name_)), defines,
                                                                               std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppTargetDSC(const string &name_, bool defines, string define,
                                                     TargetType targetType)
{
    CppSourceTarget *cppSourceTarget = &(GetCppObject(name_ + dashCpp));
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(
                                                                                   cppSourceTarget, &(GetStaticLinkOrArchiveTarget(name_)),
                                                                                   defines,
                                                                                   std::move(define)).first.operator*());
    }
    else if (targetType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(
                                                                                   cppSourceTarget, &(GetSharedLinkOrArchiveTarget(name_)),
                                                                                   defines,
                                                                                   std::move(define)).first.operator*());
    }
    else
    {
        return const_cast<DSC<CppSourceTarget> &>(
            targets<DSC<CppSourceTarget>>.emplace(cppSourceTarget).first.operator*());
    }
}

DSC<CppSourceTarget> &Configuration::GetCppStaticDSC(const string &name_, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name_ + dashCpp)),
                                                                                    &(GetStaticLinkOrArchiveTarget(name_)), defines,
                                                                                    std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppSharedDSC(const string &name_, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name_ + dashCpp)),
                                                                                    &(GetSharedLinkOrArchiveTarget(name_)), defines,
                                                                                    std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppObjectDSC(const string &name_)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name_ + dashCpp))).first.operator*());
}

DSC<CppSourceTarget, true> &Configuration::GetCppTargetDSC_P(const string &name, const string &directory, bool defines,
                                                             string define, TargetType targetType)
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

DSC<CppSourceTarget, true> &Configuration::GetCppStaticDSC_P(const string &name, const string &directory,
                                                             const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget, true> &>(
        targets<DSC<CppSourceTarget, true>>.emplace(&(GetCppObject(name + dashCpp)), &(GetStaticPrebuiltLinkOrArchiveTarget(name, directory)),
                                                    defines, std::move(define)).first.operator*());
}

DSC<CppSourceTarget, true> &Configuration::GetCppSharedDSC_P(const string &name, const string &directory,
                                                             const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget, true> &>(
        targets<DSC<CppSourceTarget, true>>.emplace(&(GetCppObject(name + dashCpp)), &(GetSharedPrebuiltLinkOrArchiveTarget(name, directory)),
                                                    defines, std::move(define)).first.operator*());
}

void Configuration::setJson()
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
}

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