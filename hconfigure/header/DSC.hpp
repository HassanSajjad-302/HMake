
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

// Dependency Specification Controller
template <typename T, bool prebuilt = false> struct DSC : DSCFeatures
{
    using BaseType = typename T::BaseType;
    ObjectFileProducerWithDS<BaseType> *objectFileProducer = nullptr;
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget = nullptr;
    PrebuiltDep prebuiltDepLocal;

    template <typename U>
    void assignLinkOrArchiveTargetLib(DSC<U> *controller, Dependency dependency, PrebuiltDep prebuiltDep);

    string define;

    DSC(T *ptr, PrebuiltLinkOrArchiveTarget *linkOrArchiveTarget_, bool defines = false, string define_ = "")
    {
        objectFileProducer = ptr;
        prebuiltLinkOrArchiveTarget = linkOrArchiveTarget_;
        if (prebuiltLinkOrArchiveTarget)
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
    }

    explicit DSC(T *ptr)
    {
        objectFileProducer = ptr;
    }

    template <typename U, typename... V> DSC<T> &PUBLIC_LIBRARIES(DSC<U> *controller, const V... libraries)
    {
        assignLinkOrArchiveTargetLib(controller, Dependency::PUBLIC, prebuiltDepLocal);
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

    template <typename U, typename... V> DSC<T> &PRIVATE_LIBRARIES(DSC<U> *controller, const V... libraries)
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

    template <typename U, typename... V> DSC<T> &INTERFACE_LIBRARIES(DSC<U> *controller, const V... libraries)
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
    DSC<T> &PUBLIC_LIBRARIES(DSC<U> *controller, PrebuiltDep prebuiltDep, const V... libraries)
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
    DSC<T> &PRIVATE_LIBRARIES(DSC<U> *controller, PrebuiltDep prebuiltDep, const V... libraries)
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
    DSC<T> &INTERFACE_LIBRARIES(DSC<U> *controller, PrebuiltDep prebuiltDep, const V... libraries)
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
    PrebuiltLinkOrArchiveTarget &getPrebuiltLinkOrArchiveTarget();
    LinkOrArchiveTarget &getLinkOrArchiveTarget();
};

template <typename T, bool prebuilt> bool operator<(const DSC<T> &lhs, const DSC<T> &rhs)
{
    return std::tie(lhs.objectFileProducer, lhs.prebuiltLinkOrArchiveTarget) <
           std::tie(rhs.objectFileProducer, rhs.prebuiltLinkOrArchiveTarget);
}

template <typename T, bool prebuilt>
template <typename U>
void DSC<T, prebuilt>::assignLinkOrArchiveTargetLib(DSC<U> *controller, Dependency dependency, PrebuiltDep prebuiltDep)
{
    // If prebuiltLinkOrArchiveTarget does not exists for a DSC<T>, then it is an ObjectLibrary.
    if (prebuiltLinkOrArchiveTarget && controller->prebuiltLinkOrArchiveTarget)
    {
        // None is ObjectLibrary
        if (prebuiltLinkOrArchiveTarget->linkTargetType == TargetType::LIBRARY_STATIC &&
            dependency == Dependency::PRIVATE)
        {
            // A static library can't have Dependency::PRIVATE deps, it can only have Dependency::INTERFACE. But, the
            // following PUBLIC_DEPS is done for correct-ordering when static-libs are finally supplied to dynamic-lib
            // or exe. Static library ignores the deps.
            prebuiltLinkOrArchiveTarget->PUBLIC_DEPS(controller->prebuiltLinkOrArchiveTarget, std::move(prebuiltDep));
        }
        else
        {
            prebuiltLinkOrArchiveTarget->DEPS(controller->prebuiltLinkOrArchiveTarget, dependency,
                                              std::move(prebuiltDep));
        }
    }
    else if (prebuiltLinkOrArchiveTarget && !controller->prebuiltLinkOrArchiveTarget)
    {
        if (!controller->objectFileProducer)
        {
            printErrorMessage(fmt::format("Dependency is nullptr\n"));
            throw std::exception();
        }
        // LinkOrArchiveTarget has ObjectLibrary as dependency.
        prebuiltLinkOrArchiveTarget->objectFileProducers.emplace(controller->objectFileProducer);
    }
    else if (!prebuiltLinkOrArchiveTarget && !controller->prebuiltLinkOrArchiveTarget)
    {
        if (!controller->objectFileProducer)
        {
            printErrorMessage(fmt::format("Dependency is nullptr\n"));
            throw std::exception();
        }
        if (!objectFileProducer)
        {
            printErrorMessage(fmt::format("Dependent is nullptr\n"));
            throw std::exception();
        }
        // ObjectLibrary has another ObjectLibrary as dependency.
        objectFileProducer->usageRequirementObjectFileProducers.emplace(controller->objectFileProducer);
    }
    else
    {
        // ObjectLibrary has LinkOrArchiveTarget as dependency.
        printErrorMessage(fmt::format(
            "ObjectLibrary DSC\n{}\ncan't have PrebuiltLinkOrArchiveTarget DSC\n{}\nas dependency.\n",
            objectFileProducer->getTarjanNodeName(), controller->prebuiltLinkOrArchiveTarget->getTarjanNodeName()));
        throw std::exception();
    }

    if (controller->defineDllInterface == DefineDLLInterface::YES)
    {
        T *ptr = static_cast<T *>(objectFileProducer);
        U *c_ptr = static_cast<U *>(controller->objectFileProducer);
        if (controller->prebuiltLinkOrArchiveTarget->EVALUATE(TargetType::LIBRARY_SHARED))
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

template <>
template <typename U>
void DSC<CSourceTarget>::assignLinkOrArchiveTargetLib(DSC<U> *controller, Dependency dependency,
                                                      PrebuiltDep prebuiltDep)
{
    // If prebuiltLinkOrArchiveTarget does not exists for a DSC<T>, then it is an ObjectLibrary.
    if (prebuiltLinkOrArchiveTarget && controller->prebuiltLinkOrArchiveTarget)
    {
        // None is ObjectLibrary
        if (prebuiltLinkOrArchiveTarget->linkTargetType == TargetType::LIBRARY_STATIC &&
            dependency == Dependency::PRIVATE)
        {
            // A static library can't have Dependency::PRIVATE deps, it can only have Dependency::INTERFACE. But, the
            // following PUBLIC_DEPS is done for correct-ordering when static-libs are finally supplied to dynamic-lib
            // or exe. Static library ignores the deps.
            prebuiltLinkOrArchiveTarget->PUBLIC_DEPS(controller->prebuiltLinkOrArchiveTarget, std::move(prebuiltDep));
        }
        else
        {
            prebuiltLinkOrArchiveTarget->DEPS(controller->prebuiltLinkOrArchiveTarget, dependency,
                                              std::move(prebuiltDep));
        }
    }
    else if (prebuiltLinkOrArchiveTarget && !controller->prebuiltLinkOrArchiveTarget)
    {
        if (!controller->objectFileProducer)
        {
            printErrorMessage(fmt::format("Dependency is nullptr\n"));
            throw std::exception();
        }
        // LinkOrArchiveTarget has ObjectLibrary as dependency.
        prebuiltLinkOrArchiveTarget->objectFileProducers.emplace(controller->objectFileProducer);
    }
    else if (!prebuiltLinkOrArchiveTarget && !controller->prebuiltLinkOrArchiveTarget)
    {
        if (!controller->objectFileProducer)
        {
            printErrorMessage(fmt::format("Dependency is nullptr\n"));
            throw std::exception();
        }
        if (!objectFileProducer)
        {
            printErrorMessage(fmt::format("Dependent is nullptr\n"));
            throw std::exception();
        }
        // ObjectLibrary has another ObjectLibrary as dependency.
        objectFileProducer->usageRequirementObjectFileProducers.emplace(controller->objectFileProducer);
    }
    else
    {
        // ObjectLibrary has LinkOrArchiveTarget as dependency.
        printErrorMessage(fmt::format(
            "ObjectLibrary DSC\n{}\ncan't have PrebuiltLinkOrArchiveTarget DSC\n{}\nas dependency.\n",
            objectFileProducer->getTarjanNodeName(), controller->prebuiltLinkOrArchiveTarget->getTarjanNodeName()));
        throw std::exception();
    }

    // Because CPT does not has the compiler, so the following is commented. Maybe a new overload which takes a bool can
    // help in a scenario where an exe has shared prebuilt as dep which has another shared prebuilt as dep Or maybe add
    // a new field consumer-compiler in CPT

    /*    if (controller->defineDllInterface == DefineDLLInterface::YES)
        {
            CPT *ptr = static_cast<CPT *>(objectFileProducer);
            U *c_ptr = static_cast<U *>(controller->objectFileProducer);
            if (controller->prebuiltLinkOrArchiveTarget->EVALUATE(TargetType::LIBRARY_SHARED))
            {
                if (ptr->compiler.bTFamily == BTFamily::MSVC)
                {
                    c_ptr->usageRequirementCompileDefinitions.emplace(Define(controller->define,
       "__declspec(dllimport)"));
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
        }*/
}

template <typename T, bool prebuilt> T &DSC<T, prebuilt>::getSourceTarget()
{
    return static_cast<T &>(*objectFileProducer);
}

template <typename T, bool prebuilt> T *DSC<T, prebuilt>::getSourceTargetPointer()
{
    return static_cast<T *>(objectFileProducer);
}

template <typename T, bool prebuilt> PrebuiltLinkOrArchiveTarget &DSC<T, prebuilt>::getPrebuiltLinkOrArchiveTarget()
{
    return *prebuiltLinkOrArchiveTarget;
}

template <typename T, bool prebuilt> LinkOrArchiveTarget &DSC<T, prebuilt>::getLinkOrArchiveTarget()
{
    return static_cast<LinkOrArchiveTarget &>(*prebuiltLinkOrArchiveTarget);
}

template <>
DSC<CppSourceTarget>::DSC(class CppSourceTarget *ptr, PrebuiltLinkOrArchiveTarget *linkOrArchiveTarget_, bool defines,
                          string define_);
#endif // HMAKE_DSC_HPP
