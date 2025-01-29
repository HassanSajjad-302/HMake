
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
    // Pointer is unused beside CppSourceTarget *
    T *stored = nullptr;
    // These pointers are assigned in the constructor. An invariant is that these can not nullptr.
    ObjectFileProducerWithDS<BaseType> *objectFileProducer = nullptr;
    PrebuiltBasic *prebuiltBasic = nullptr;
    PrebuiltDep prebuiltDepLocal;

    template <typename U, typename... V>
    void assignLinkOrArchiveTargetLib(Dependency dependency, DSC<U> *controller, PrebuiltDep prebuiltDep, V... args);

    template <typename U, typename... V>
    void assignObjectFileProducerDeps(Dependency dependency, DSC<U> *controller, V... args);

    template <typename U, typename... V>
    void assignLinkOrArchiveTargetLib(Dependency dependency, DSC<U> *controller, V... args);
    DSC &save(CppSourceTarget *ptr);
    DSC &saveAndReplace(CppSourceTarget *ptr);
    DSC &restore();

    string define;

    DSC(T *ptr, PrebuiltBasic *prebuiltBasic_, bool defines = false, string define_ = "")
    {
        objectFileProducer = ptr;
        prebuiltBasic = prebuiltBasic_;
        prebuiltBasic->objectFileProducers.emplace(objectFileProducer);

        if (define_.empty() && !prebuiltBasic->evaluate(TargetType::LIBRARY_OBJECT))
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

    template <typename U, typename... V> DSC &publicLibraries(DSC<U> *controller, const V... libraries)
    {
        assignLinkOrArchiveTargetLib(Dependency::PUBLIC, controller, libraries...);
        return *this;
    }

    template <typename U, typename... V> DSC &privateLibraries(DSC<U> *controller, const V... libraries)
    {
        assignLinkOrArchiveTargetLib(Dependency::PRIVATE, controller, libraries...);
        return *this;
    }

    template <typename U, typename... V> DSC &INTERFACE_LIBRARIES(DSC<U> *controller, const V... libraries)
    {
        assignLinkOrArchiveTargetLib(Dependency::INTERFACE, controller, libraries...);
        return *this;
    }

    template <typename U, typename... V>
    DSC &publicLibraries(DSC<U> *controller, PrebuiltDep prebuiltDep, const V... libraries)
    {
        assignLinkOrArchiveTargetLib(Dependency::PUBLIC, controller, std::move(prebuiltDep), libraries...);
        return *this;
    }

    template <typename U, typename... V>
    DSC &privateLibraries(DSC<U> *controller, PrebuiltDep prebuiltDep, const V... libraries)
    {
        assignLinkOrArchiveTargetLib(Dependency::PRIVATE, controller, std::move(prebuiltDep), libraries...);
        return *this;
    }

    template <typename U, typename... V>
    DSC &INTERFACE_LIBRARIES(DSC<U> *controller, PrebuiltDep prebuiltDep, const V... libraries)
    {
        assignLinkOrArchiveTargetLib(Dependency::INTERFACE, controller, std::move(prebuiltDep), libraries...);
        return *this;
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
template <typename U, typename... V>
void DSC<T>::assignObjectFileProducerDeps(Dependency dependency, DSC<U> *controller, V... args)
{
    objectFileProducer->DEPS(controller->getSourceTargetPointer(), dependency);

    // TODO
    // A limitation is that this might add two different compile definitions for two different consumers with different
    // compilers. i.e. if gcc and msvc both are consuming a library on Windows. An alternative of GenerateExportHeader
    // CMake module like functionality is to be supported as well.

    if (controller->defineDllInterface == DefineDLLInterface::YES)
    {
        T *ptr = static_cast<T *>(objectFileProducer);
        U *c_ptr = static_cast<U *>(controller->objectFileProducer);
        if (controller->prebuiltBasic->evaluate(TargetType::LIBRARY_SHARED))
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
template <typename U, typename... V>
void DSC<T>::assignLinkOrArchiveTargetLib(Dependency dependency, DSC<U> *controller, PrebuiltDep prebuiltDep, V... args)
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

    assignObjectFileProducerDeps(dependency, controller);

    if constexpr (sizeof...(args))
    {
        return assignLinkOrArchiveTargetLib(dependency, args...);
    }
}

template <typename T>
template <typename U, typename... V>
void DSC<T>::assignLinkOrArchiveTargetLib(Dependency dependency, DSC<U> *controller, V... args)
{
    assignLinkOrArchiveTargetLib(dependency, controller, prebuiltDepLocal);

    if constexpr (sizeof...(args))
    {
        return assignLinkOrArchiveTargetLib(dependency, args...);
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
DSC<CppSourceTarget>::DSC(class CppSourceTarget *ptr, PrebuiltBasic *prebuiltBasic_, bool defines, string define_);

template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::save(CppSourceTarget *ptr);
template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::saveAndReplace(CppSourceTarget *ptr);
template <> DSC<CppSourceTarget> &DSC<CppSourceTarget>::restore();

#endif // HMAKE_DSC_HPP
