
#ifdef USE_HEADER_UNITS
import "DSC.hpp";
#else
#include "DSC.hpp"
#endif

template <>
DSC<CppSourceTarget>::DSC(CppSourceTarget *ptr, PrebuiltLinkOrArchiveTarget *linkOrArchiveTarget_, bool defines,
                          string define_)
{
    objectFileProducer = ptr;
    prebuiltLinkOrArchiveTarget = linkOrArchiveTarget_;
    if (linkOrArchiveTarget_)
    {
        prebuiltLinkOrArchiveTarget->objectFileProducers.emplace(objectFileProducer);
    }

    if (define_.empty() && prebuiltLinkOrArchiveTarget)
    {
        define = prebuiltLinkOrArchiveTarget->outputName;
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
        if (prebuiltLinkOrArchiveTarget && prebuiltLinkOrArchiveTarget->EVALUATE(TargetType::LIBRARY_SHARED))
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