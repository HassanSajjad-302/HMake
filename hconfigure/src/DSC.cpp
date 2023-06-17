
#ifdef USE_HEADER_UNITS
import "DSC.hpp";
#else
#include "DSC.hpp"
#endif

template <> DSC<CppSourceTarget>::DSC(CppSourceTarget *ptr, PrebuiltBasic *prebuiltBasic_, bool defines, string define_)
{
    // TODO Remove this later
    if (!ptr || !prebuiltBasic_)
    {
        printErrorMessage("Error in Specialized DSC constructor. One is nullptr\n");
        throw std::exception{};
    }

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