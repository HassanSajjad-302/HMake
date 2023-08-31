
#ifdef USE_HEADER_UNITS
import "DSC.hpp";
#else
#include "DSC.hpp"
#endif

template <>
DSC<CppSourceTarget>::DSC(CppSourceTarget *ptr, PrebuiltBasic *prebuiltBasic_, bool defines, pstring define_)
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
        transform(define.begin(), define.end(), define.begin(), ::toupper);
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

template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::push(CppSourceTarget *ptr)
{
    if (!pushed)
    {
        pushed = static_cast<CppSourceTarget *>(objectFileProducer);
    }
    objectFileProducer = ptr;
    return *this;
}

template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::pushAndInitialize(CppSourceTarget *ptr)
{
    push(ptr);
    for (const SMFile &smFile : pushed->moduleSourceFileDependencies)
    {
        ptr->moduleSourceFileDependencies.emplace(ptr, const_cast<Node *>(smFile.node));
    }
    return *this;
}

template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::pop()
{
    objectFileProducer = pushed;
    return *this;
}