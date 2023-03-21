
#ifndef HMAKE_DSC_HPP
#define HMAKE_DSC_HPP
#ifdef USE_HEADER_UNITS
import "LinkOrArchiveTarget.hpp";
#else
#include "LinkOrArchiveTarget.hpp"
#endif

// Dependency Specification Controller
template <typename T> struct DSC
{
    using BaseType = T::BaseType;
    ObjectFileProducerWithDS<BaseType> *objectFileProducer = nullptr;
    LinkOrArchiveTarget *linkOrArchiveTarget = nullptr;
    auto operator<=>(const DSC<T> &) const = default;

    void assignLinkOrArchiveTargetLib(DSC *controller);

    template <same_as<DSC<T> *>... U> DSC<T> &PUBLIC_LIBRARIES(DSC<T> *controller, const U... libraries)
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

    template <same_as<DSC<T> *>... U> DSC<T> &PRIVATE_LIBRARIES(DSC<T> *controller, const U... libraries)
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

    template <same_as<DSC<T> *>... U> DSC<T> &INTERFACE_LIBRARIES(DSC<T> *controller, const U... libraries)
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

    T &getSourceTarget();
    T *getSourceTargetPointer();
};

template <typename T> void DSC<T>::assignLinkOrArchiveTargetLib(DSC *controller)
{
    if (!objectFileProducer || !controller->objectFileProducer)
    {
        print(stderr, "DSC<T> objectFileProducer cannot be nullptr\n");
        exit(EXIT_FAILURE);
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
        print(stderr, "ObjectLibrary DSC\n{}\ncan't have LinkOrArchiveTarget DSC\n{}\nas dependency.\n",
              objectFileProducer->getTarjanNodeName(), controller->linkOrArchiveTarget->getTarjanNodeName());
        exit(EXIT_FAILURE);
    }
}

/*template <typename T>
template <same_as<DSC<T> *>... U>
DSC<T> &DSC<T>::PUBLIC_LIBRARIES(DSC<T> *controller, const U... libraries)
{

}

template <typename T>
template <same_as<DSC<T> *>... U>
DSC<T> &DSC<T>::PRIVATE_LIBRARIES(DSC<T> *controller, const U... libraries)
{

    return *this;
}

template <typename T>
template <same_as<DSC<T> *>... U>
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

#endif // HMAKE_DSC_HPP

