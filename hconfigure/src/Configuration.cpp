#include "Configuration.hpp"
#include "CppSourceTarget.hpp"
#include "LinkOrArchiveTarget.hpp"

Configuration::Configuration(const string &name_) : CTarget{name_}
{
}

Configuration::Configuration(const string &name_, CTarget &other, bool hasFile) : CTarget{name_, other, hasFile}
{
}

CppSourceTarget &Configuration::GetPreProcessCpp(const string &name_)
{
    CppSourceTarget &cppSourceTarget = const_cast<CppSourceTarget &>(
        cppSourceTargets
            .emplace(name_ + "-cpp", TargetType::PREPROCESS, *this, configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator*());
    static_cast<CommonFeatures &>(cppSourceTarget) = commonFeatures;
    static_cast<CompilerFeatures &>(cppSourceTarget) = compilerFeatures;
    return cppSourceTarget;
}

CppSourceTarget &Configuration::GetCompileCpp(const string &name_)
{
    CppSourceTarget &cppSourceTarget = const_cast<CppSourceTarget &>(
        cppSourceTargets
            .emplace(name_ + "-cpp", TargetType::COMPILE, *this, configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator*());
    static_cast<CommonFeatures &>(cppSourceTarget) = commonFeatures;
    static_cast<CompilerFeatures &>(cppSourceTarget) = compilerFeatures;
    return cppSourceTarget;
}

LinkOrArchiveTarget &Configuration::GetExe(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = const_cast<LinkOrArchiveTarget &>(
        linkOrArchiveTargets
            .emplace(name_, TargetType::EXECUTABLE, *this, configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator*());
    static_cast<CommonFeatures &>(linkOrArchiveTarget) = commonFeatures;
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    return linkOrArchiveTarget;
}

LinkOrArchiveTarget &Configuration::GetLib(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = const_cast<LinkOrArchiveTarget &>(
        linkOrArchiveTargets.emplace(name_, cache.libraryType, *this, configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator*());
    static_cast<CommonFeatures &>(linkOrArchiveTarget) = commonFeatures;
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    return linkOrArchiveTarget;
}

CppSourceTarget &Configuration::GetExeCpp(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = GetExe(name_);
    static_cast<CommonFeatures &>(linkOrArchiveTarget) = commonFeatures;
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        cppSourceTargets
            .emplace(name_ + "-cpp", TargetType::COMPILE, linkOrArchiveTarget, linkOrArchiveTarget,
                     configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator->());
    static_cast<CommonFeatures &>(*cppSourceTarget) = commonFeatures;
    static_cast<CompilerFeatures &>(*cppSourceTarget) = compilerFeatures;
    linkOrArchiveTarget.cppSourceTarget = cppSourceTarget;
    return *cppSourceTarget;
}

LinkOrArchiveTarget &Configuration::GetCppExe(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = GetExe(name_);
    static_cast<CommonFeatures &>(linkOrArchiveTarget) = commonFeatures;
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        cppSourceTargets
            .emplace(name_ + "-cpp", TargetType::COMPILE, linkOrArchiveTarget, linkOrArchiveTarget,
                     configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator->());
    static_cast<CommonFeatures &>(*cppSourceTarget) = commonFeatures;
    static_cast<CompilerFeatures &>(*cppSourceTarget) = compilerFeatures;
    linkOrArchiveTarget.cppSourceTarget = cppSourceTarget;
    return linkOrArchiveTarget;
}

CppSourceTarget &Configuration::GetLibCpp(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = GetLib(name_);
    static_cast<CommonFeatures &>(linkOrArchiveTarget) = commonFeatures;
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        cppSourceTargets
            .emplace(name_ + "-cpp", TargetType::COMPILE, linkOrArchiveTarget, linkOrArchiveTarget,
                     configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator->());
    static_cast<CommonFeatures &>(*cppSourceTarget) = commonFeatures;
    static_cast<CompilerFeatures &>(*cppSourceTarget) = compilerFeatures;
    linkOrArchiveTarget.cppSourceTarget = cppSourceTarget;
    return *cppSourceTarget;
}

LinkOrArchiveTarget &Configuration::GetCppLib(const string &name_)
{
    LinkOrArchiveTarget &linkOrArchiveTarget = GetLib(name_);
    static_cast<CommonFeatures &>(linkOrArchiveTarget) = commonFeatures;
    static_cast<LinkerFeatures &>(linkOrArchiveTarget) = linkerFeatures;
    CppSourceTarget *cppSourceTarget = const_cast<CppSourceTarget *>(
        cppSourceTargets
            .emplace(name_ + "-cpp", TargetType::COMPILE, linkOrArchiveTarget, linkOrArchiveTarget,
                     configTargetHaveFile == ConfigTargetHaveFile::YES)
            .first.
            operator->());
    static_cast<CommonFeatures &>(*cppSourceTarget) = commonFeatures;
    static_cast<CompilerFeatures &>(*cppSourceTarget) = compilerFeatures;
    linkOrArchiveTarget.cppSourceTarget = cppSourceTarget;
    return linkOrArchiveTarget;
}

void Configuration::setJson()
{
    Json variantJson;
    variantJson[JConsts::configuration] = commonFeatures.configurationType;
    variantJson[JConsts::compiler] = compilerFeatures.compiler;
    variantJson[JConsts::linker] = linkerFeatures.linker;
    variantJson[JConsts::archiver] = linkerFeatures.archiver;
    variantJson[JConsts::compilerFlags] = compilerFeatures.privateCompilerFlags;
    variantJson[JConsts::compileDefinitions] = compilerFeatures.privateCompileDefinitions;
    variantJson[JConsts::linkerFlags] = linkerFeatures.privateLinkerFlags;
    variantJson[JConsts::libraryType] = linkerFeatures.libraryType;
    variantJson[JConsts::targets] = elements;
    json[0] = std::move(variantJson);
}