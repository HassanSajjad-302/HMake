
#ifndef HMAKE_DSC_HPP
#define HMAKE_DSC_HPP
#ifdef USE_HEADER_UNITS
import "LinkOrArchiveTarget.hpp";
#else
#include "LinkOrArchiveTarget.hpp"
#endif

template <typename T> struct DSCPrebuilt;

// Dependency Specification Controller
template <typename T> struct DSC
{
    using BaseType = T::BaseType;
    ObjectFileProducerWithDS<BaseType> *objectFileProducer = nullptr;
    LinkOrArchiveTarget *linkOrArchiveTarget = nullptr;
    auto operator<=>(const DSC<T> &) const = default;

    void assignLinkOrArchiveTargetLib(DSC *controller);
    void assignPrebuiltLinkOrArchiveTarget(DSCPrebuilt<BaseType> *controller);
    template <typename... U> DSC<T> &PUBLIC_LIBRARIES(DSC<T> *controller, const U... libraries)
    {
        assignLinkOrArchiveTargetLib(controller);
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
        assignLinkOrArchiveTargetLib(controller);
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
        assignLinkOrArchiveTargetLib(controller);
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
        assignPrebuiltLinkOrArchiveTarget(controller);
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
        assignPrebuiltLinkOrArchiveTarget(controller);
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
        assignPrebuiltLinkOrArchiveTarget(controller);
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

template <typename T> void DSC<T>::assignLinkOrArchiveTargetLib(DSC *controller)
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
            linkOrArchiveTarget->INTERFACE_DEPS(controller->linkOrArchiveTarget);
        }
        else
        {
            linkOrArchiveTarget->PRIVATE_DEPS(controller->linkOrArchiveTarget);
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
        printErrorMessage(format("ObjectLibrary DSC\n{}\ncan't have LinkOrArchiveTarget DSC\n{}\nas dependency.\n",
                                 objectFileProducer->getTarjanNodeName(),
                                 controller->linkOrArchiveTarget->getTarjanNodeName()));
        throw std::exception();
    }
}

template <typename T> void DSC<T>::assignPrebuiltLinkOrArchiveTarget(DSCPrebuilt<BaseType> *controller)
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
            linkOrArchiveTarget->INTERFACE_DEPS(controller->prebuiltLinkOrArchiveTarget);
        }
        else
        {
            linkOrArchiveTarget->PRIVATE_DEPS(controller->prebuiltLinkOrArchiveTarget);
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
template <typename T> struct DSCPrebuilt
{
    T *prebuilt = nullptr;
    PrebuiltLinkOrArchiveTarget *prebuiltLinkOrArchiveTarget = nullptr;
    auto operator<=>(const DSCPrebuilt<T> &) const = default;

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

#endif // HMAKE_DSC_HPP
