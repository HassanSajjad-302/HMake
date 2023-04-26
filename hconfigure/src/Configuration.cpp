
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

void Configuration::setModuleScope(CppSourceTarget *moduleScope)
{
    for (CppSourceTarget *cppSourceTargetLocal : cppSourceTargets)
    {
        cppSourceTargetLocal->setModuleScope(moduleScope);
    }
}

CppSourceTarget &Configuration::GetCppPreprocess(const string &name_)
{
    CppSourceTarget &cppSourceTarget = const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>
            .emplace(name_ , TargetType::PREPROCESS, *this, configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator*());
    cppSourceTargets.emplace_back(&cppSourceTarget);
    static_cast<CompilerFeatures &>(cppSourceTarget) = compilerFeatures;
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
    static_cast<CompilerFeatures &>(cppSourceTarget) = compilerFeatures;
    return cppSourceTarget;
}

LinkOrArchiveTarget &Configuration::GetExe(const string &name_)
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

LinkOrArchiveTarget &Configuration::GetStatic(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name_, TargetType::LIBRARY_STATIC, *this, configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator*());
    linkOrArchiveTargets.emplace_back(&linkOrArchiveTarget);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::GetShared(const string &name_)
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

CPT &Configuration::GetCPT()
{
    CPT &cpt = const_cast<CPT &>(targets<CPT>.emplace().first.operator*());
    prebuiltTargets.emplace_back(&cpt);
    return cpt;
}

DSC<CppSourceTarget> &Configuration::GetCppExeDSC(const string &name_, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(
                                                                               &(GetCppObject(name_ + dashCpp)),
                                                                               &(GetExe(name_)), defines,
                                                                               std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppTargetDSC(const string &name_, bool defines, string define,
                                                     TargetType targetType)
{
    CppSourceTarget *cppSourceTarget = &(GetCppObject(name_ + dashCpp));
    if (targetType == TargetType::LIBRARY_STATIC)
    {
        return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(
                                                                                   cppSourceTarget, &(GetStatic(name_)),
                                                                                   defines,
                                                                                   std::move(define)).first.operator*());
    }
    else if (targetType == TargetType::LIBRARY_SHARED)
    {
        return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(
                                                                                   cppSourceTarget, &(GetShared(name_)),
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
                                                                                    &(GetStatic(name_)), defines,
                                                                                    std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppSharedDSC(const string &name_, const bool defines, string define)
{
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name_ + dashCpp)),
                                                                                    &(GetShared(name_)), defines,
                                                                                    std::move(define)).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppObjectDSC(const string &name_)
{
    return const_cast<DSC<CppSourceTarget> &>(
        targets<DSC<CppSourceTarget>>.emplace(&(GetCppObject(name_ + dashCpp))).first.operator*());
}

DSCPrebuilt<CPT> &Configuration::GetCPTTargetDSC(const string &name_, const string &directory,
                                                 TargetType linkTargetType_)
{
    DSCPrebuilt<CPT> dsc(&(GetCPT()), &(GetPrebuiltLinkOrArchiveTarget(name_, directory, linkTargetType_)));
    return const_cast<DSCPrebuilt<CPT> &>(targets<DSCPrebuilt<CPT>>.emplace(dsc).first.operator*());
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