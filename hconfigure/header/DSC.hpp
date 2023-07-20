
#ifndef HMAKE_DSC_HPP
#define HMAKE_DSC_HPP
#ifdef USE_HEADER_UNITS
import "LinkOrArchiveTarget.hpp";
import "ObjectFileProducer.hpp";
import "CppSourceTarget.hpp";
#else
#include "CppSourceTarget.hpp"
#include "LinkOrArchiveTarget.hpp"
#include "ObjectFileProducer.hpp"
#endif

// Dependency Specification Controller. Following declaration is for T = CSourceTarget
template <typename T> struct DSC : DSCFeatures
{
    using BaseType = typename T::BaseType;
    ObjectFileProducerWithDS<BaseType> *objectFileProducer = nullptr;
    PrebuiltBasic *prebuiltBasic = nullptr;
    PrebuiltDep prebuiltDepLocal;

    template <typename U>
    void assignLinkOrArchiveTargetLib(DSC<U> *controller, Dependency dependency, PrebuiltDep prebuiltDep);

    pstring define;

    DSC(T *ptr, PrebuiltBasic *prebuiltBasic_, bool defines = false, pstring define_ = "")
    {
        if (!ptr || !prebuiltBasic_)
        {
            printErrorMessage("Error in General DSC constructor. One is nullptr\n");
            throw std::exception{};
        }
        objectFileProducer = ptr;
        prebuiltBasic = prebuiltBasic_;
        prebuiltBasic->objectFileProducers.emplace(objectFileProducer);

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
    }

    template <typename U, typename... V> DSC &PUBLIC_LIBRARIES(DSC<U> *controller, const V... libraries)
    {
        assignLinkOrArchiveTargetLib(controller, Dependency::PUBLIC, prebuiltDepLocal);
        // TODO
        //  This is an unnecessary if as constructor is already checking for nullptr. Also, this code can be moved in
        //  assignLinkOrArchiveTarget as well.
        if (objectFileProducer && controller->objectFileProducer)
        {
            objectFileProducer->PUBLIC_DEPS(controller->getSourceTargetPointer());
        }
        if constexpr (sizeof...(libraries))
        {
            return PUBLIC_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename U, typename... V> DSC &PRIVATE_LIBRARIES(DSC<U> *controller, const V... libraries)
    {
        assignLinkOrArchiveTargetLib(controller, Dependency::PRIVATE, prebuiltDepLocal);
        if (objectFileProducer && controller->objectFileProducer)
        {
            objectFileProducer->PRIVATE_DEPS(controller->getSourceTargetPointer());
        }
        if constexpr (sizeof...(libraries))
        {
            return PRIVATE_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename U, typename... V> DSC &INTERFACE_LIBRARIES(DSC<U> *controller, const V... libraries)
    {
        assignLinkOrArchiveTargetLib(controller, Dependency::INTERFACE, prebuiltDepLocal);
        if (objectFileProducer && controller->objectFileProducer)
        {
            objectFileProducer->INTERFACE_DEPS(controller->getSourceTargetPointer());
        }
        if constexpr (sizeof...(libraries))
        {
            return INTERFACE_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename U, typename... V>
    DSC &PUBLIC_LIBRARIES(DSC<U> *controller, PrebuiltDep prebuiltDep, const V... libraries)
    {
        assignLinkOrArchiveTargetLib(controller, Dependency::PUBLIC, std::move(prebuiltDep));
        if (objectFileProducer && controller->objectFileProducer)
        {
            objectFileProducer->PUBLIC_DEPS(controller->getSourceTargetPointer());
        }
        if constexpr (sizeof...(libraries))
        {
            return PUBLIC_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename U, typename... V>
    DSC &PRIVATE_LIBRARIES(DSC<U> *controller, PrebuiltDep prebuiltDep, const V... libraries)
    {
        assignLinkOrArchiveTargetLib(controller, Dependency::PRIVATE, std::move(prebuiltDep));
        if (objectFileProducer && controller->objectFileProducer)
        {
            objectFileProducer->PRIVATE_DEPS(controller->getSourceTargetPointer());
        }
        if constexpr (sizeof...(libraries))
        {
            return PRIVATE_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename U, typename... V>
    DSC &INTERFACE_LIBRARIES(DSC<U> *controller, PrebuiltDep prebuiltDep, const V... libraries)
    {
        assignLinkOrArchiveTargetLib(controller, Dependency::INTERFACE, std::move(prebuiltDep));
        if (objectFileProducer && controller->objectFileProducer)
        {
            objectFileProducer->INTERFACE_DEPS(controller->getSourceTargetPointer());
        }
        if constexpr (sizeof...(libraries))
        {
            return INTERFACE_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    T &getSourceTarget();
    T *getSourceTargetPointer();
    PrebuiltBasic &getPrebuiltBasicTarget();
    PrebuiltLinkOrArchiveTarget &getPrebuiltLinkOrArchiveTarget();
    LinkOrArchiveTarget &getLinkOrArchiveTarget();
};

template <typename T> bool operator<(const DSC<T> &lhs, const DSC<T> &rhs)
{
    return std::tie(lhs.objectFileProducer, lhs.prebuiltBasic) < std::tie(rhs.objectFileProducer, rhs.prebuiltBasic);
}

template <typename T>
template <typename U>
void DSC<T>::assignLinkOrArchiveTargetLib(DSC<U> *controller, Dependency dependency, PrebuiltDep prebuiltDep)
{
    if (prebuiltBasic->linkTargetType != TargetType::LIBRARY_SHARED && dependency == Dependency::PRIVATE)
    {
        // A static library or object library can't have Dependency::PRIVATE deps, it can only have
        // Dependency::INTERFACE. But, the following PUBLIC_DEPS is done for correct-ordering when static-libs are
        // finally supplied to dynamic-lib or exe. Static library ignores the deps.
        prebuiltBasic->PUBLIC_DEPS(controller->prebuiltBasic, std::move(prebuiltDep));
    }
    else
    {
        prebuiltBasic->DEPS(controller->prebuiltBasic, dependency, std::move(prebuiltDep));
    }

    // TODO
    // A limitation is that this might add two different compile definitions for two different consumers with different
    // compilers. i.e. if gcc and msvc both are consuming a library on Windows. An alternative of GenerateExportHeader
    // CMake module like functionality is to be supported as well.

    if (controller->defineDllInterface == DefineDLLInterface::YES)
    {
        T *ptr = static_cast<T *>(objectFileProducer);
        U *c_ptr = static_cast<U *>(controller->objectFileProducer);
        if (controller->prebuiltBasic->EVALUATE(TargetType::LIBRARY_SHARED))
        {
            if (ptr->compiler.bTFamily == BTFamily::MSVC)
            {
                c_ptr->usageRequirementCompileDefinitions.emplace(Define(controller->define, "__declspec(dllimport)"));
            }
            else
            {
                c_ptr->usageRequirementCompileDefinitions.emplace(Define(controller->define, ""));
            }
        }
        else
        {
            c_ptr->usageRequirementCompileDefinitions.emplace(Define(controller->define, ""));
        }
    }
}

template <typename T> T &DSC<T>::getSourceTarget()
{
    return static_cast<T &>(*objectFileProducer);
}

template <typename T> T *DSC<T>::getSourceTargetPointer()
{
    return static_cast<T *>(objectFileProducer);
}

template <typename T> PrebuiltBasic &DSC<T>::getPrebuiltBasicTarget()
{
    return *prebuiltBasic;
}

template <typename T> PrebuiltLinkOrArchiveTarget &DSC<T>::getPrebuiltLinkOrArchiveTarget()
{
    return static_cast<PrebuiltLinkOrArchiveTarget &>(*prebuiltBasic);
}

template <typename T> LinkOrArchiveTarget &DSC<T>::getLinkOrArchiveTarget()
{
    return static_cast<LinkOrArchiveTarget &>(*prebuiltBasic);
}

template <>
DSC<CppSourceTarget>::DSC(class CppSourceTarget *ptr, PrebuiltBasic *prebuiltBasic_, bool defines, pstring define_);

#endif // HMAKE_DSC_HPP
