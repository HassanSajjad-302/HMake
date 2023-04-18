
#ifndef HMAKE_DSC_HPP
#define HMAKE_DSC_HPP
#ifdef USE_HEADER_UNITS
import "LinkOrArchiveTarget.hpp";
#else
#include "LinkOrArchiveTarget.hpp"
#endif

template <typename T> struct DSCPrebuilt;

// Dependency Specification Controller
template <typename T> struct DSC : DSCFeatures
{
    using BaseType = typename T::BaseType;
    ObjectFileProducerWithDS<BaseType> *objectFileProducer = nullptr;
    LinkOrArchiveTarget *linkOrArchiveTarget = nullptr;
    PrebuiltDep prebuiltDepLocal;

    void assignLinkOrArchiveTargetLib(DSC *controller, Dependency dependency, PrebuiltDep prebuiltDep);
    void assignPrebuiltLinkOrArchiveTarget(DSCPrebuilt<BaseType> *controller, Dependency dependency,
                                           PrebuiltDep prebuiltDep);

    string define;

    DSC(T *ptr, LinkOrArchiveTarget *linkOrArchiveTarget_, bool defines = false, string define_ = "")
    {
        objectFileProducer = ptr;
        linkOrArchiveTarget = linkOrArchiveTarget_;
        linkOrArchiveTarget->objectFileProducers.emplace(objectFileProducer);

        if (define_.empty())
        {
            define = linkOrArchiveTarget->name;
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
            // Maybe call a function pointer in ptr if user wants to customize this
            if (linkOrArchiveTarget->EVALUATE(TargetType::LIBRARY_SHARED))
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

    DSC(T *ptr)
    {
        objectFileProducer = ptr;
    }

    template <typename... U> DSC<T> &PUBLIC_LIBRARIES(DSC<T> *controller, const U... libraries)
    {
        assignLinkOrArchiveTargetLib(controller, Dependency::PUBLIC, prebuiltDepLocal);
        objectFileProducer->PUBLIC_DEPS(controller->getSourceTargetPointer());
        if constexpr (sizeof...(libraries))
        {
            return PUBLIC_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename... U> DSC<T> &PRIVATE_LIBRARIES(DSC<T> *controller, const U... libraries)
    {
        assignLinkOrArchiveTargetLib(controller, Dependency::PRIVATE, prebuiltDepLocal);
        objectFileProducer->PRIVATE_DEPS(controller->getSourceTargetPointer());
        if constexpr (sizeof...(libraries))
        {
            return PRIVATE_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename... U> DSC<T> &INTERFACE_LIBRARIES(DSC<T> *controller, const U... libraries)
    {
        assignLinkOrArchiveTargetLib(controller, Dependency::INTERFACE, prebuiltDepLocal);
        objectFileProducer->INTERFACE_DEPS(controller->getSourceTargetPointer());
        if constexpr (sizeof...(libraries))
        {
            return INTERFACE_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename... U> DSC<T> &PUBLIC_LIBRARIES(DSCPrebuilt<BaseType> *controller, const U... libraries)
    {
        assignPrebuiltLinkOrArchiveTarget(controller, Dependency::PUBLIC, prebuiltDepLocal);
        objectFileProducer->PUBLIC_DEPS(controller->getSourceTargetPointer());
        if constexpr (sizeof...(libraries))
        {
            return PUBLIC_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename... U> DSC<T> &PRIVATE_LIBRARIES(DSCPrebuilt<BaseType> *controller, const U... libraries)
    {
        assignPrebuiltLinkOrArchiveTarget(controller, Dependency::PRIVATE, prebuiltDepLocal);
        objectFileProducer->PRIVATE_DEPS(controller->getSourceTargetPointer());
        if constexpr (sizeof...(libraries))
        {
            return PRIVATE_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename... U> DSC<T> &INTERFACE_LIBRARIES(DSCPrebuilt<BaseType> *controller, const U... libraries)
    {
        assignPrebuiltLinkOrArchiveTarget(controller, Dependency::INTERFACE, prebuiltDepLocal);
        objectFileProducer->INTERFACE_DEPS(controller->getSourceTargetPointer());
        if constexpr (sizeof...(libraries))
        {
            return INTERFACE_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename... U> DSC<T> &PUBLIC_LIBRARIES(DSC<T> *controller, PrebuiltDep prebuiltDep, const U... libraries)
    {
        assignLinkOrArchiveTargetLib(controller, Dependency::PUBLIC, std::move(prebuiltDep));
        objectFileProducer->PUBLIC_DEPS(controller->getSourceTargetPointer());
        if constexpr (sizeof...(libraries))
        {
            return PUBLIC_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename... U>
    DSC<T> &PRIVATE_LIBRARIES(DSC<T> *controller, PrebuiltDep prebuiltDep, const U... libraries)
    {
        assignLinkOrArchiveTargetLib(controller, Dependency::PRIVATE, std::move(prebuiltDep));
        objectFileProducer->PRIVATE_DEPS(controller->getSourceTargetPointer());
        if constexpr (sizeof...(libraries))
        {
            return PRIVATE_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename... U>
    DSC<T> &INTERFACE_LIBRARIES(DSC<T> *controller, PrebuiltDep prebuiltDep, const U... libraries)
    {
        assignLinkOrArchiveTargetLib(controller, Dependency::INTERFACE, std::move(prebuiltDep));
        objectFileProducer->INTERFACE_DEPS(controller->getSourceTargetPointer());
        if constexpr (sizeof...(libraries))
        {
            return INTERFACE_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename... U>
    DSC<T> &PUBLIC_LIBRARIES(DSCPrebuilt<BaseType> *controller, PrebuiltDep prebuiltDep, const U... libraries)
    {
        assignPrebuiltLinkOrArchiveTarget(controller, Dependency::PUBLIC, std::move(prebuiltDep));
        objectFileProducer->PUBLIC_DEPS(controller->getSourceTargetPointer());
        if constexpr (sizeof...(libraries))
        {
            return PUBLIC_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename... U>
    DSC<T> &PRIVATE_LIBRARIES(DSCPrebuilt<BaseType> *controller, PrebuiltDep prebuiltDep, const U... libraries)
    {
        assignPrebuiltLinkOrArchiveTarget(controller, Dependency::PRIVATE, std::move(prebuiltDep));
        objectFileProducer->PRIVATE_DEPS(controller->getSourceTargetPointer());
        if constexpr (sizeof...(libraries))
        {
            return PRIVATE_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename... U>
    DSC<T> &INTERFACE_LIBRARIES(DSCPrebuilt<BaseType> *controller, PrebuiltDep prebuiltDep, const U... libraries)
    {
        assignPrebuiltLinkOrArchiveTarget(controller, Dependency::INTERFACE, std::move(prebuiltDep));
        objectFileProducer->INTERFACE_DEPS(controller->getSourceTargetPointer());
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
};

template <typename T> bool operator<(const DSC<T> &lhs, const DSC<T> &rhs)
{
    return std::tie(lhs.objectFileProducer, lhs.linkOrArchiveTarget) <
           std::tie(rhs.objectFileProducer, rhs.linkOrArchiveTarget);
}

template <typename T>
void DSC<T>::assignLinkOrArchiveTargetLib(DSC *controller, Dependency dependency, PrebuiltDep prebuiltDep)
{
    if (!objectFileProducer || !controller->objectFileProducer)
    {
        printErrorMessage("DSC<T> objectFileProducer cannot be nullptr\n");
        throw std::exception();
    }
    // If linkOrArchiveTarget does not exists for a DSC<T>, then it is an ObjectLibrary.
    if (linkOrArchiveTarget && controller->linkOrArchiveTarget)
    {
        // None is ObjectLibrary
        if (linkOrArchiveTarget->linkTargetType == TargetType::LIBRARY_STATIC)
        {
            linkOrArchiveTarget->INTERFACE_DEPS(controller->linkOrArchiveTarget, std::move(prebuiltDep));
        }
        else
        {
            linkOrArchiveTarget->DEPS(controller->linkOrArchiveTarget, dependency, std::move(prebuiltDep));
        }
    }
    else if (linkOrArchiveTarget && !controller->linkOrArchiveTarget)
    {
        // LinkOrArchiveTarget has ObjectLibrary as dependency.
        linkOrArchiveTarget->objectFileProducers.emplace(controller->objectFileProducer);
    }
    else if (!linkOrArchiveTarget && !controller->linkOrArchiveTarget)
    {
        // ObjectLibrary has another ObjectLibrary as dependency.
        objectFileProducer->usageRequirementObjectFileProducers.emplace(controller->objectFileProducer);
    }
    else
    {
        // ObjectLibrary has LinkOrArchiveTarget as dependency.
        printErrorMessage(fmt::format("ObjectLibrary DSC\n{}\ncan't have LinkOrArchiveTarget DSC\n{}\nas dependency.\n",
                                      objectFileProducer->getTarjanNodeName(),
                                      controller->linkOrArchiveTarget->getTarjanNodeName()));
        throw std::exception();
    }

    if (controller->defineDllInterface == DefineDLLInterface::YES)
    {
        T *ptr = static_cast<T *>(objectFileProducer);
        T *c_ptr = static_cast<T *>(controller->objectFileProducer);
        if (controller->linkOrArchiveTarget->EVALUATE(TargetType::LIBRARY_SHARED))
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

template <typename T>
void DSC<T>::assignPrebuiltLinkOrArchiveTarget(DSCPrebuilt<BaseType> *controller, Dependency dependency,
                                               PrebuiltDep prebuiltDep)
{
    if (!objectFileProducer || !controller->prebuilt)
    {
        printErrorMessage("DSC<T> objectFileProducer  or DSCPrebuilt<T> prebuilt cannot be nullptr\n");
        throw std::exception();
    }
    if (!linkOrArchiveTarget || !controller->prebuiltLinkOrArchiveTarget)
    {
        printErrorMessage(
            "DSC<T> linkOrArchiveTarget or DSCPrebuilt<T> prebuiltLinkOrArchiveTarget cannot be nullptr\n");
        throw std::exception();
    }
    // If linkOrArchiveTarget does not exists for a DSC<T>, then it is an ObjectLibrary.
    if (linkOrArchiveTarget && controller->prebuiltLinkOrArchiveTarget)
    {
        // None is ObjectLibrary
        if (linkOrArchiveTarget->linkTargetType == TargetType::LIBRARY_STATIC)
        {
            linkOrArchiveTarget->INTERFACE_DEPS(controller->prebuiltLinkOrArchiveTarget, std::move(prebuiltDep));
        }
        else
        {
            linkOrArchiveTarget->DEPS(controller->prebuiltLinkOrArchiveTarget, std::move(prebuiltDep), dependency);
        }
    }

    if (controller->defineDllInterface == DefineDLLInterface::YES)
    {
        T *ptr = static_cast<T *>(objectFileProducer);
        BaseType *c_ptr = controller->prebuilt;
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

/*template <typename T>
template <typename... U>
DSC<T> &DSC<T>::PUBLIC_LIBRARIES(DSC<T> *controller, const U... libraries)
{

}

template <typename T>
template <typename... U>
DSC<T> &DSC<T>::PRIVATE_LIBRARIES(DSC<T> *controller, const U... libraries)
{

    return *this;
}

template <typename T>
template <typename... U>
DSC<T> &DSC<T>::INTERFACE_LIBRARIES(DSC<T> *controller, const U... libraries)
{

    return *this;
}*/

template <typename T> T &DSC<T>::getSourceTarget()
{
    return static_cast<T &>(*objectFileProducer);
}

template <typename T> T *DSC<T>::getSourceTargetPointer()
{
    return static_cast<T *>(objectFileProducer);
}

// Dependency Specification Controller Prebuilt
template <typename T> struct DSCPrebuilt : DSCPrebuiltFeatures
{
    T *prebuilt = nullptr;
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget = nullptr;

    string define;

    DSCPrebuilt(T *ptr, PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget_, bool defines = false,
                string define_ = "")
    {

        prebuilt = ptr;
        prebuiltLinkOrArchiveTarget = prebuiltLinkOrArchiveTarget_;

        if (define_.empty())
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
            defineDllInterface = DefineDLLInterface::YES;
        }
    }

    template <typename... U> DSCPrebuilt<T> &INTERFACE_LIBRARIES(DSCPrebuilt<T> *controller, const U... libraries)
    {
        prebuilt->INTERFACE_DEPS(controller->getSourceTargetPointer());
        prebuiltLinkOrArchiveTarget->PRIVATE_DEPS(controller->getSourceTargetPointer());
        if constexpr (sizeof...(libraries))
        {
            return INTERFACE_LIBRARIES(libraries...);
        }
        else
        {
            return *this;
        }
    }

    template <typename... U>
    DSCPrebuilt<T> &INTERFACE_LIBRARIES(DSCPrebuilt<T> *controller, PrebuiltDep prebuiltDep, const U... libraries)
    {
        prebuilt->INTERFACE_DEPS(controller->getSourceTargetPointer(), prebuiltDep);
        prebuiltLinkOrArchiveTarget->PRIVATE_DEPS(controller->getSourceTargetPointer());
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
    PrebuiltLinkOrArchiveTarget &getLinkTarget();
    PrebuiltLinkOrArchiveTarget *getLinkTargetPointer();
};

template <typename T> T &DSCPrebuilt<T>::getSourceTarget()
{
    return static_cast<T &>(*prebuilt);
}

template <typename T> T *DSCPrebuilt<T>::getSourceTargetPointer()
{
    return static_cast<T *>(prebuilt);
}

template <typename T> PrebuiltLinkOrArchiveTarget &DSCPrebuilt<T>::getLinkTarget()
{
    return *prebuiltLinkOrArchiveTarget;
}

template <typename T> PrebuiltLinkOrArchiveTarget *DSCPrebuilt<T>::getLinkTargetPointer()
{
    return prebuiltLinkOrArchiveTarget;
}

template <typename T> bool operator<(const DSCPrebuilt<T> &lhs, const DSCPrebuilt<T> &rhs)
{
    return lhs.prebuiltLinkOrArchiveTarget < rhs.prebuiltLinkOrArchiveTarget;
}

#endif // HMAKE_DSC_HPP
