
#ifndef HMAKE_DSC_HPP
#define HMAKE_DSC_HPP
#ifdef USE_HEADER_UNITS
import "Features.hpp";
import "ObjectFileProducer.hpp";
#else
#include "Features.hpp"
#include "ObjectFileProducer.hpp"
#endif

class CppSourceTarget;
class LOAT;

// Dependency Specification Controller. Following declaration is for T = CSourceTarget
template <typename T> struct DSC : DSCFeatures
{
    using BaseType = typename T::BaseType;
    // Pointer is unused beside CppSourceTarget *
    T *stored = nullptr;
    // These pointers are assigned in the constructor. An invariant is that these can not nullptr.
    ObjectFileProducerWithDS<BaseType> *objectFileProducer = nullptr;
    PLOAT *ploat = nullptr;
    PrebuiltDep prebuiltDepLocal;

    template <typename U, typename... V>
    void assignLOATLib(Dependency dependency, DSC<U> *controller, PrebuiltDep prebuiltDep, V... args);

    template <typename U> void assignObjectFileProducerDeps(Dependency dependency, DSC<U> *controller);

    template <typename U, typename... V> void assignLOATLib(Dependency dependency, DSC<U> *controller, V... args);
    DSC &save(CppSourceTarget *ptr);
    DSC &saveAndReplace(CppSourceTarget *ptr);
    DSC &restore();

    string define;

    DSC(T *ptr, PLOAT *ploat_, bool defines = false, string define_ = "")
    {
        objectFileProducer = ptr;
        ploat = ploat_;
        ploat->objectFileProducers.emplace(objectFileProducer);

        if (define_.empty())
        {
            define = ploat->getOutputName();
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
        assignLOATLib(Dependency::PUBLIC, controller, libraries...);
        return *this;
    }

    template <typename U, typename... V> DSC &privateLibraries(DSC<U> *controller, const V... libraries)
    {
        assignLOATLib(Dependency::PRIVATE, controller, libraries...);
        return *this;
    }

    template <typename U, typename... V> DSC &INTERFACE_LIBRARIES(DSC<U> *controller, const V... libraries)
    {
        assignLOATLib(Dependency::INTERFACE, controller, libraries...);
        return *this;
    }

    template <typename U, typename... V>
    DSC &publicLibraries(DSC<U> *controller, PrebuiltDep prebuiltDep, const V... libraries)
    {
        assignLOATLib(Dependency::PUBLIC, controller, std::move(prebuiltDep), libraries...);
        return *this;
    }

    template <typename U, typename... V>
    DSC &privateLibraries(DSC<U> *controller, PrebuiltDep prebuiltDep, const V... libraries)
    {
        assignLOATLib(Dependency::PRIVATE, controller, std::move(prebuiltDep), libraries...);
        return *this;
    }

    template <typename U, typename... V>
    DSC &INTERFACE_LIBRARIES(DSC<U> *controller, PrebuiltDep prebuiltDep, const V... libraries)
    {
        assignLOATLib(Dependency::INTERFACE, controller, std::move(prebuiltDep), libraries...);
        return *this;
    }

    T &getSourceTarget();
    T *getSourceTargetPointer();
    PLOAT &getPLOATTarget();
    PLOAT &getPLOAT();
    LOAT &getLOAT();
};

template <typename T> bool operator<(const DSC<T> &lhs, const DSC<T> &rhs)
{
    return std::tie(lhs.objectFileProducer, lhs.ploat) < std::tie(rhs.objectFileProducer, rhs.ploat);
}

template <typename T>
template <typename U>
void DSC<T>::assignObjectFileProducerDeps(Dependency dependency, DSC<U> *controller)
{
    objectFileProducer->deps(controller->getSourceTargetPointer(), dependency);

    if (controller->defineDllInterface == DefineDLLInterface::YES)
    {
        T *ptr = static_cast<T *>(objectFileProducer);

        Define define;
        define.name = controller->define;

        if (controller->ploat->evaluate(TargetType::LIBRARY_SHARED))
        {
            if (ptr->configuration->compilerFeatures.compiler.bTFamily == BTFamily::MSVC)
            {
                define.value = "__declspec(dllimport)";
            }
        }

        if (dependency == Dependency::PUBLIC)
        {
            ptr->reqCompileDefinitions.emplace(define);
            ptr->useReqCompileDefinitions.emplace(define);
        }
        else if (dependency == Dependency::PRIVATE)
        {
            ptr->reqCompileDefinitions.emplace(define);
        }
        else
        {
            ptr->useReqCompileDefinitions.emplace(define);
        }
    }
}

template <typename T>
template <typename U, typename... V>
void DSC<T>::assignLOATLib(Dependency dependency, DSC<U> *controller, PrebuiltDep prebuiltDep, V... args)
{
    if (ploat->linkTargetType != TargetType::LIBRARY_SHARED && dependency == Dependency::PRIVATE)
    {
        // A static library or object library can't have Dependency::PRIVATE deps, it can only have
        // Dependency::INTERFACE. But, the following publicDeps is done for correct-ordering when static-libs are
        // finally supplied to dynamic-lib or exe. Static library ignores the deps.
        ploat->publicDeps(controller->ploat, std::move(prebuiltDep));
    }
    else
    {
        ploat->deps(controller->ploat, dependency, std::move(prebuiltDep));
    }

    assignObjectFileProducerDeps(dependency, controller);

    if constexpr (sizeof...(args))
    {
        return assignLOATLib(dependency, args...);
    }
}

template <typename T>
template <typename U, typename... V>
void DSC<T>::assignLOATLib(Dependency dependency, DSC<U> *controller, V... args)
{
    assignLOATLib(dependency, controller, prebuiltDepLocal);

    if constexpr (sizeof...(args))
    {
        return assignLOATLib(dependency, args...);
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

template <typename T> PLOAT &DSC<T>::getPLOATTarget()
{
    return *ploat;
}

template <typename T> PLOAT &DSC<T>::getPLOAT()
{
    return static_cast<PLOAT &>(*ploat);
}

template <typename T> LOAT &DSC<T>::getLOAT()
{
    return static_cast<LOAT &>(*ploat);
}

#endif // HMAKE_DSC_HPP
