
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
template <typename T, bool prebuilt = false> struct DSC : DSCFeatures
{
    using BaseType = typename T::BaseType;
    ObjectFileProducerWithDS<BaseType> *objectFileProducer = nullptr;
    PrebuiltBasic *prebuiltBasic = nullptr;
    PrebuiltDep prebuiltDepLocal;

    template <typename U, bool prebuilt_>
    void assignLinkOrArchiveTargetLib(DSC<U, prebuilt_> *controller, Dependency dependency, PrebuiltDep prebuiltDep);

    // Overload for prebuilt libraries will always specify the objectFileProducer of the dependency to the
    // linkOrArchiveTarget because that objectFileProducer can't be consumed by PrebuiltLinkOrArchiveTarget as it is
    // already built. Necessary for drop-in replacement of header-files with header-units of a
    // prebuiltLinkorArchiveTarget.
    /*
        template <typename U>
        void assignLinkOrArchiveTargetLib(DSC<U, true> *controller, Dependency dependency, PrebuiltDep prebuiltDep);
    */

    string define;

    DSC(T *ptr, PrebuiltBasic *prebuiltBasic_, bool defines = false, string define_ = "")
    {
        // TODO Remove this later
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

    template <typename U, bool prebuilt_, typename... V>
    DSC &PUBLIC_LIBRARIES(DSC<U, prebuilt_> *controller, const V... libraries)
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

    template <typename U, bool prebuilt_, typename... V>
    DSC &PRIVATE_LIBRARIES(DSC<U, prebuilt_> *controller, const V... libraries)
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

    template <typename U, bool prebuilt_, typename... V>
    DSC &INTERFACE_LIBRARIES(DSC<U, prebuilt_> *controller, const V... libraries)
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

    template <typename U, bool prebuilt_, typename... V>
    DSC &PUBLIC_LIBRARIES(DSC<U, prebuilt_> *controller, PrebuiltDep prebuiltDep, const V... libraries)
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

    template <typename U, bool prebuilt_, typename... V>
    DSC &PRIVATE_LIBRARIES(DSC<U, prebuilt_> *controller, PrebuiltDep prebuiltDep, const V... libraries)
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

    template <typename U, bool prebuilt_, typename... V>
    DSC &INTERFACE_LIBRARIES(DSC<U, prebuilt_> *controller, PrebuiltDep prebuiltDep, const V... libraries)
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

template <typename T, bool prebuilt> bool operator<(const DSC<T, prebuilt> &lhs, const DSC<T, prebuilt> &rhs)
{
    return std::tie(lhs.objectFileProducer, lhs.prebuiltBasic) < std::tie(rhs.objectFileProducer, rhs.prebuiltBasic);
}

template <typename T, bool prebuilt>
template <typename U, bool prebuilt_>
void DSC<T, prebuilt>::assignLinkOrArchiveTargetLib(DSC<U, prebuilt_> *controller, Dependency dependency,
                                                    PrebuiltDep prebuiltDep)
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

    /*    else if (prebuiltBasic)
        {
            if (!controller->objectFileProducer)
            {
                printErrorMessage(fmt::format("Dependency is nullptr\n"));
                throw std::exception();
            }
            // LinkOrArchiveTarget has ObjectLibrary as dependency.
            prebuiltBasic->objectFileProducers.emplace(controller->objectFileProducer);
        }
        else if (controller->prebuiltBasic)
        {
            // ObjectLibrary has LinkOrArchiveTarget as dependency.
            printErrorMessage(fmt::format(
                "ObjectLibrary DSC\n{}\ncan't have PrebuiltLinkOrArchiveTarget DSC\n{}\nas dependency.\n",
                objectFileProducer->getTarjanNodeName(), controller->prebuiltBasic->getTarjanNodeName()));
            throw std::exception();
        }
        else
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
        }*/

    // A limitation is that this might add two different compiledefinitions for two different consumers with different
    // compilers. A solution is to use a header-file with definitions as CMake does.

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

/*
template <typename T, bool prebuilt>
template <typename U>
void DSC<T, prebuilt>::assignLinkOrArchiveTargetLib(DSC<U, true> *controller, Dependency dependency,
                                                    PrebuiltDep prebuiltDep)
{
    // If prebuiltLinkOrArchiveTarget does not exists for a DSC, then it is an ObjectLibrary.
    if (prebuiltBasic && controller->prebuiltBasic)
    {
        // None is ObjectLibrary
        if (prebuiltBasic->linkTargetType == TargetType::LIBRARY_STATIC &&
            dependency == Dependency::PRIVATE)
        {
            // A static library can't have Dependency::PRIVATE deps, it can only have Dependency::INTERFACE. But, the
            // following PUBLIC_DEPS is done for correct-ordering when static-libs are finally supplied to dynamic-lib
            // or exe. Static library ignores the deps.
            prebuiltBasic->PUBLIC_DEPS(controller->prebuiltBasic, std::move(prebuiltDep));
        }
        else
        {
            prebuiltBasic->DEPS(controller->prebuiltBasic, dependency,
                                              std::move(prebuiltDep));
        }
        prebuiltBasic->objectFileProducers.emplace(controller->objectFileProducer);
    }
    else
    {
        if (prebuiltBasic)
        {
            if (!controller->objectFileProducer)
            {
                printErrorMessage(fmt::format("Dependency is nullptr\n"));
                throw std::exception();
            }
            // LinkOrArchiveTarget has ObjectLibrary as dependency.
            prebuiltBasic->objectFileProducers.emplace(controller->objectFileProducer);
        }
        else if (!prebuiltBasic && !controller->prebuiltBasic)
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
                objectFileProducer->getTarjanNodeName(), controller->prebuiltBasic->getTarjanNodeName()));
            throw std::exception();
        }
    }

    if (controller->defineDllInterface == DefineDLLInterface::YES)
    {
        T *ptr = static_cast<T *>(objectFileProducer);
        U *c_ptr = static_cast<U *>(controller->objectFileProducer);
        if (prebuiltBasic &&
            controller->prebuiltBasic->EVALUATE(TargetType::LIBRARY_SHARED))
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
*/

template <>
template <typename U, bool prebuilt_>
void DSC<CSourceTarget>::assignLinkOrArchiveTargetLib(DSC<U, prebuilt_> *controller, Dependency dependency,
                                                      PrebuiltDep prebuiltDep)
{
    // None is ObjectLibrary
    if (prebuiltBasic->linkTargetType != TargetType::LIBRARY_SHARED && dependency == Dependency::PRIVATE)
    {
        // A static library can't have Dependency::PRIVATE deps, it can only have Dependency::INTERFACE. But, the
        // following PUBLIC_DEPS is done for correct-ordering when static-libs are finally supplied to dynamic-lib
        // or exe. Static library ignores the deps.
        prebuiltBasic->PUBLIC_DEPS(controller->prebuiltBasic, std::move(prebuiltDep));
    }
    else
    {
        prebuiltBasic->DEPS(controller->prebuiltBasic, dependency, std::move(prebuiltDep));
    }

    /*    else if (prebuiltBasic && !controller->prebuiltBasic)
        {
            if (!controller->objectFileProducer)
            {
                printErrorMessage(fmt::format("Dependency is nullptr\n"));
                throw std::exception();
            }
            // LinkOrArchiveTarget has ObjectLibrary as dependency.
            prebuiltBasic->objectFileProducers.emplace(controller->objectFileProducer);
        }
        else if (!prebuiltBasic && !controller->prebuiltBasic)
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
                objectFileProducer->getTarjanNodeName(), controller->prebuiltBasic->getTarjanNodeName()));
            throw std::exception();
        }*/

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

template <typename T, bool prebuilt> PrebuiltBasic &DSC<T, prebuilt>::getPrebuiltBasicTarget()
{
    return *prebuiltBasic;
}

template <typename T, bool prebuilt> PrebuiltLinkOrArchiveTarget &DSC<T, prebuilt>::getPrebuiltLinkOrArchiveTarget()
{
    return static_cast<PrebuiltLinkOrArchiveTarget &>(*prebuiltBasic);
}

template <typename T, bool prebuilt> LinkOrArchiveTarget &DSC<T, prebuilt>::getLinkOrArchiveTarget()
{
    return static_cast<LinkOrArchiveTarget &>(*prebuiltBasic);
}

template <>
DSC<CppSourceTarget>::DSC(class CppSourceTarget *ptr, PrebuiltBasic *prebuiltBasic_, bool defines, string define_);

#endif // HMAKE_DSC_HPP
