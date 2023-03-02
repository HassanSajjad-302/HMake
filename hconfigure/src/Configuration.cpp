#include "Configuration.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include "DSC.hpp"
#include "LinkOrArchiveTarget.hpp"

Configuration::Configuration(const string &name_) : CTarget{name_}
{
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
    static_cast<CommonFeatures &>(cppSourceTarget) = commonFeatures;
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
    static_cast<CommonFeatures &>(cppSourceTarget) = commonFeatures;
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
    static_cast<CommonFeatures &>(linkOrArchiveTarget) = commonFeatures;
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
    static_cast<CommonFeatures &>(linkOrArchiveTarget) = commonFeatures;
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
    static_cast<CommonFeatures &>(linkOrArchiveTarget) = commonFeatures;
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    return linkOrArchiveTarget;
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

void Configuration::setJson()
{
    Json variantJson;
    variantJson[JConsts::configuration] = commonFeatures.configurationType;
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