
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
    selectiveBuild = isCTargetInSelectedSubDirectory();
}

Configuration::Configuration(const string &name_, CTarget &other, bool hasFile) : CTarget{name_, other, hasFile}
{
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
    cppSourceTargets.emplace(&cppSourceTarget);
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
    cppSourceTargets.emplace(&cppSourceTarget);
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
    linkOrArchiveTargets.emplace(&linkOrArchiveTarget);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::GetStatic(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name_, TargetType::LIBRARY_STATIC, *this, configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator*());
    linkOrArchiveTargets.emplace(&linkOrArchiveTarget);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::GetShared(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name_, TargetType::LIBRARY_SHARED, *this, configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator*());
    linkOrArchiveTargets.emplace(&linkOrArchiveTarget);
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    return linkOrArchiveTarget;
}

PrebuiltLinkOrArchiveTarget &Configuration::GetPrebuiltLinkOrArchiveTarget(const string &name, const string &directory,
                                                                           TargetType linkTargetType_)
{
    PrebuiltLinkOrArchiveTarget &prebuiltLinkOrArchiveTarget = const_cast<PrebuiltLinkOrArchiveTarget &>(
        targets<PrebuiltLinkOrArchiveTarget>.emplace(name, directory, linkTargetType_).first.operator*());
    prebuiltLinkOrArchiveTargets.emplace(&prebuiltLinkOrArchiveTarget);
    return prebuiltLinkOrArchiveTarget;
}

CPT &Configuration::GetCPT()
{
    CPT &cpt = const_cast<CPT &>(targets<CPT>.emplace().first.operator*());
    prebuiltTargets.emplace(&cpt);
    return cpt;
}

DSC<CppSourceTarget> &Configuration::GetCppExeDSC(const string &name_)
{
    DSC<CppSourceTarget> dsc;
    dsc.objectFileProducer = &(GetCppObject(name_ + dashCpp));
    dsc.linkOrArchiveTarget = &(GetExe(name_));
    dsc.linkOrArchiveTarget->objectFileProducers.emplace(dsc.objectFileProducer);
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(dsc).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppDSC(const string &name_)
{
    DSC<CppSourceTarget> dsc;
    dsc.objectFileProducer = &(GetCppObject(name_ + dashCpp));
    if (cache.libraryType == TargetType::LIBRARY_STATIC)
    {
        dsc.linkOrArchiveTarget = &(GetStatic(name_));
        dsc.linkOrArchiveTarget->objectFileProducers.emplace(dsc.objectFileProducer);
    }
    else if (cache.libraryType == TargetType::LIBRARY_SHARED)
    {
        dsc.linkOrArchiveTarget = &(GetShared(name_));
        dsc.linkOrArchiveTarget->objectFileProducers.emplace(dsc.objectFileProducer);
    }
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(dsc).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppStaticDSC(const string &name_)
{
    DSC<CppSourceTarget> dsc;
    dsc.objectFileProducer = &(GetCppObject(name_ + dashCpp));
    dsc.linkOrArchiveTarget = &(GetStatic(name_));
    dsc.linkOrArchiveTarget->objectFileProducers.emplace(dsc.objectFileProducer);
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(dsc).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppSharedDSC(const string &name_)
{
    DSC<CppSourceTarget> dsc;
    dsc.objectFileProducer = &(GetCppObject(name_ + dashCpp));
    dsc.linkOrArchiveTarget = &(GetShared(name_));
    dsc.linkOrArchiveTarget->objectFileProducers.emplace(dsc.objectFileProducer);
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(dsc).first.operator*());
}

DSC<CppSourceTarget> &Configuration::GetCppObjectDSC(const string &name_)
{
    DSC<CppSourceTarget> dsc;
    dsc.objectFileProducer = &(GetCppObject(name_ + dashCpp));
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(dsc).first.operator*());
}

DSCPrebuilt<CPT> &Configuration::GetCPTDSC(const string &name, const string &directory, TargetType linkTargetType_)
{
    DSCPrebuilt<CPT> dsc;
    dsc.prebuilt = &(GetCPT());
    dsc.prebuiltLinkOrArchiveTarget = &(GetPrebuiltLinkOrArchiveTarget(name, directory, linkTargetType_));
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
