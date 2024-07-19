
#ifdef USE_HEADER_UNITS
import "DSC.hpp";
#else
#include "DSC.hpp"
#endif

template <>
DSC<CppSourceTarget>::DSC(CppSourceTarget *ptr, PrebuiltBasic *prebuiltBasic_, const bool defines, pstring define_)
{
    objectFileProducer = ptr;
    prebuiltBasic = prebuiltBasic_;
    if (prebuiltBasic_)
    {
        prebuiltBasic->objectFileProducers.emplace(objectFileProducer);
    }

    if (define_.empty() && !prebuiltBasic->EVALUATE(TargetType::PREBUILT_BASIC))
    {
        define = prebuiltBasic->outputName;
        transform(define.begin(), define.end(), define.begin(), toupper);
        define += "_EXPORT";
    }
    else
    {
        define = std::move(define_);
    }

    if (defines)
    {
        defineDllPrivate = DefineDLLPrivate::YES;
        defineDllInterface = DefineDLLInterface::YES;
    }

    if (defineDllPrivate == DefineDLLPrivate::YES)
    {
        if (prebuiltBasic->EVALUATE(TargetType::LIBRARY_SHARED))
        {
            if (ptr->compiler.bTFamily == BTFamily::MSVC)
            {
                ptr->requirementCompileDefinitions.emplace(Define(define, "__declspec(dllexport)"));
            }
            else
            {
                ptr->requirementCompileDefinitions.emplace(
                    Define(define, "\"__attribute__ ((visibility (\\\"default\\\")))\""));
            }
        }
        else
        {
            ptr->requirementCompileDefinitions.emplace(Define(define, ""));
        }
    }
}

template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::save(CppSourceTarget *ptr)
{
    if (!stored)
    {
        stored = static_cast<CppSourceTarget *>(objectFileProducer);
    }
    objectFileProducer = ptr;
    return *this;
}

template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::saveAndReplace(CppSourceTarget *ptr)
{
    save(ptr);
    for (const SMFile &smFile : stored->moduleSourceFileDependencies)
    {
        if (smFile.isInterface)
        {
            ptr->moduleSourceFileDependencies.emplace(ptr, const_cast<Node *>(smFile.node));
        }
    }
    for (auto &[inclNode, cppSourceTarget] : stored->requirementHuDirs)
    {
        ptr->requirementHuDirs.emplace(inclNode, ptr);
    }
    for (auto &[inclNode, cppSourceTarget] : stored->usageRequirementHuDirs)
    {
        ptr->usageRequirementHuDirs.emplace(inclNode, ptr);
    }
    ptr->requirementCompileDefinitions = stored->requirementCompileDefinitions;
    ptr->requirementIncludes = stored->requirementIncludes;

    ptr->usageRequirementCompileDefinitions = stored->usageRequirementCompileDefinitions;
    ptr->usageRequirementIncludes = stored->usageRequirementIncludes;
    return *this;
}

template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::restore()
{
    objectFileProducer = stored;
    return *this;
}