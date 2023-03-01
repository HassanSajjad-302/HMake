
#include "GetTarget.hpp"
#include "BuildSystemFunctions.hpp"
#include "Cache.hpp"
#include "CppSourceTarget.hpp"

// TODO
//  Why _CPP_SOURCE is embedded with name?

CppSourceTarget &GetCppPreprocess(const string &name)
{

    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name, TargetType::PREPROCESS).first.operator*());
}

CppSourceTarget &GetCppPreprocess(const string &name, CTarget &other, bool hasFile)
{

    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name, TargetType::PREPROCESS, other, hasFile).first.operator*());
}

CppSourceTarget &GetCppObject(const string &name)
{
    return const_cast<CppSourceTarget &>(targets<CppSourceTarget>.emplace(name, TargetType::OBJECT).first.operator*());
}

CppSourceTarget &GetCppObject(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<CppSourceTarget &>(
        targets<CppSourceTarget>.emplace(name, TargetType::OBJECT, other, hasFile).first.operator*());
}

LinkOrArchiveTarget &GetExe(const string &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::EXECUTABLE).first.operator*());
}

LinkOrArchiveTarget &GetExe(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::EXECUTABLE, other, hasFile).first.operator*());
}

LinkOrArchiveTarget &GetStatic(const string &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::LIBRARY_STATIC).first.operator*());
}

LinkOrArchiveTarget &GetStatic(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::LIBRARY_STATIC, other, hasFile).first.operator*());
}

LinkOrArchiveTarget &GetShared(const string &name)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::LIBRARY_SHARED).first.operator*());
}

LinkOrArchiveTarget &GetShared(const string &name, CTarget &other, bool hasFile)
{
    return const_cast<LinkOrArchiveTarget &>(
        targets<LinkOrArchiveTarget>.emplace(name, TargetType::LIBRARY_SHARED, other, hasFile).first.operator*());
}

DSC<CppSourceTarget> &GetCppExeDSC(const string &name)
{
    DSC<CppSourceTarget> dsc;
    dsc.linkOrArchiveTarget = &(GetExe(name + dashLink));
    dsc.objectFileProducer = &(GetCppObject(name + dashCpp));
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace().first.operator*());
}

DSC<CppSourceTarget> &GetCppExeDSC(const string &name, CTarget &other, bool hasFile)
{
    DSC<CppSourceTarget> dsc;
    dsc.linkOrArchiveTarget = &(GetExe(name + dashLink, other, hasFile));
    dsc.objectFileProducer = &(GetCppObject(name + dashCpp, other, hasFile));
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace().first.operator*());
}

DSC<CppSourceTarget> &GetCppDSC(const string &name)
{
    DSC<CppSourceTarget> dsc;
    dsc.objectFileProducer = &(GetCppObject(name + dashCpp));
    if (cache.libraryType == TargetType::LIBRARY_STATIC)
    {
        dsc.linkOrArchiveTarget = &(GetStatic(name + dashLink));
    }
    else if (cache.libraryType == TargetType::LIBRARY_SHARED)
    {
        dsc.linkOrArchiveTarget = &(GetShared(name + dashLink));
    }
    dsc.linkOrArchiveTarget->objectFileProducers.emplace(dsc.objectFileProducer);
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(dsc).first.operator*());
}

DSC<CppSourceTarget> &GetCppDSC(const string &name, CTarget &other, bool hasFile)
{
    DSC<CppSourceTarget> dsc;
    dsc.objectFileProducer = &(GetCppObject(name + dashCpp, other, hasFile));
    if (cache.libraryType == TargetType::LIBRARY_STATIC)
    {
        dsc.linkOrArchiveTarget = &(GetStatic(name + dashLink, other, hasFile));
    }
    else if (cache.libraryType == TargetType::LIBRARY_SHARED)
    {
        dsc.linkOrArchiveTarget = &(GetShared(name + dashLink, other, hasFile));
    }
    dsc.linkOrArchiveTarget->objectFileProducers.emplace(dsc.objectFileProducer);
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(dsc).first.operator*());
}

DSC<CppSourceTarget> &GetCppStaticDSC(const string &name)
{
    DSC<CppSourceTarget> dsc;
    dsc.linkOrArchiveTarget = &(GetStatic(name + dashLink));
    dsc.objectFileProducer = &(GetCppObject(name + dashCpp));
    dsc.linkOrArchiveTarget->objectFileProducers.emplace(dsc.objectFileProducer);
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(dsc).first.operator*());
}

DSC<CppSourceTarget> &GetCppStaticDSC(const string &name, CTarget &other, bool hasFile)
{
    DSC<CppSourceTarget> dsc;
    dsc.linkOrArchiveTarget = &(GetStatic(name + dashLink, other, hasFile));
    dsc.objectFileProducer = &(GetCppObject(name + dashCpp, other, hasFile));
    dsc.linkOrArchiveTarget->objectFileProducers.emplace(dsc.objectFileProducer);
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(dsc).first.operator*());
}

DSC<CppSourceTarget> &GetCppSharedDSC(const string &name)
{
    DSC<CppSourceTarget> dsc;
    dsc.linkOrArchiveTarget = &(GetShared(name + dashLink));
    dsc.objectFileProducer = &(GetCppObject(name + dashCpp));
    dsc.linkOrArchiveTarget->objectFileProducers.emplace(dsc.objectFileProducer);
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(dsc).first.operator*());
}

DSC<CppSourceTarget> &GetCppSharedDSC(const string &name, CTarget &other, bool hasFile)
{
    DSC<CppSourceTarget> dsc;
    dsc.linkOrArchiveTarget = &(GetShared(name + dashLink, other, hasFile));
    dsc.objectFileProducer = &(GetCppObject(name + dashCpp, other, hasFile));
    dsc.linkOrArchiveTarget->objectFileProducers.emplace(dsc.objectFileProducer);
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace(dsc).first.operator*());
}

DSC<CppSourceTarget> &GetCppObjectDSC(const string &name)
{
    DSC<CppSourceTarget> dsc;
    dsc.objectFileProducer = &(GetCppObject(name + dashCpp));
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace().first.operator*());
}

DSC<CppSourceTarget> &GetCppObjectDSC(const string &name, CTarget &other, bool hasFile)
{
    DSC<CppSourceTarget> dsc;
    dsc.objectFileProducer = &(GetCppObject(name + dashCpp, other, hasFile));
    return const_cast<DSC<CppSourceTarget> &>(targets<DSC<CppSourceTarget>>.emplace().first.operator*());
}