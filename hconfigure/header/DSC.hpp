
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
    void assignLOATDep(DepType depType, DSC<U> &dsc, PrebuiltDep prebuiltDep, V... args);

    template <typename U> void assignObjectFileProducerDeps(DepType depType, DSC<U> &dsc);

    template <typename U, typename... V> void assignLOATDep(DepType depType, DSC<U> &dsc, V... args);
    DSC &save(CppSourceTarget &ptr);
    DSC &saveAndReplace(CppSourceTarget &ptr);
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

    template <typename U, typename... V> DSC &publicDeps(DSC<U> &dsc, const V... dscs)
    {
        assignLOATDep(DepType::PUBLIC, dsc, dscs...);
        return *this;
    }

    template <typename U, typename... V> DSC &privateDeps(DSC<U> &dsc, const V... dscs)
    {
        assignLOATDep(DepType::PRIVATE, dsc, dscs...);
        return *this;
    }

    template <typename U, typename... V> DSC &interfaceDeps(DSC<U> &dsc, const V... dscs)
    {
        assignLOATDep(DepType::INTERFACE, dsc, dscs...);
        return *this;
    }

    template <typename U, typename... V> DSC &deps(DepType depType, DSC<U> &dsc, const V... dscs)
    {
        assignLOATDep(depType, dsc, dscs...);
        return *this;
    }

    template <typename U, typename... V> DSC &publicDeps(DSC<U> &dsc, PrebuiltDep prebuiltDep, const V... dscs)
    {
        assignLOATDep(DepType::PUBLIC, dsc, std::move(prebuiltDep), dscs...);
        return *this;
    }

    template <typename U, typename... V> DSC &privateDeps(DSC<U> &dsc, PrebuiltDep prebuiltDep, const V... dscs)
    {
        assignLOATDep(DepType::PRIVATE, dsc, std::move(prebuiltDep), dscs...);
        return *this;
    }

    template <typename U, typename... V> DSC &interfaceDeps(DSC<U> &dsc, PrebuiltDep prebuiltDep, const V... dscs)
    {
        assignLOATDep(DepType::INTERFACE, dsc, std::move(prebuiltDep), dscs...);
        return *this;
    }

    template <typename U, typename... V>
    DSC &deps(DepType depType, DSC<U> &dsc, PrebuiltDep prebuiltDep, const V... dscs)
    {
        assignLOATDep(depType, dsc, std::move(prebuiltDep), dscs...);
        return *this;
    }

    T &getSourceTarget();
    T *getSourceTargetPointer();
    PLOAT &getPLOAT() const;
    LOAT &getLOAT();
};

template <typename T> bool operator<(const DSC<T> &lhs, const DSC<T> &rhs)
{
    return std::tie(lhs.objectFileProducer, lhs.ploat) < std::tie(rhs.objectFileProducer, rhs.ploat);
}

template <typename T> template <typename U> void DSC<T>::assignObjectFileProducerDeps(DepType depType, DSC<U> &dsc)
{
    objectFileProducer->deps(depType, dsc.getSourceTarget());

    if (dsc.defineDllInterface == DefineDLLInterface::YES)
    {
        T *ptr = static_cast<T *>(objectFileProducer);

        Define define;
        define.name = dsc.define;

        if (dsc.ploat->evaluate(TargetType::LIBRARY_SHARED))
        {
            if (ptr->configuration->compilerFeatures.compiler.bTFamily == BTFamily::MSVC)
            {
                define.value = "__declspec(dllimport)";
            }
        }

        if (depType == DepType::PUBLIC)
        {
            ptr->reqCompileDefinitions.emplace(define);
            ptr->useReqCompileDefinitions.emplace(define);
        }
        else if (depType == DepType::PRIVATE)
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
void DSC<T>::assignLOATDep(DepType depType, DSC<U> &dsc, PrebuiltDep prebuiltDep, V... args)
{
    if (ploat->linkTargetType != TargetType::LIBRARY_SHARED && depType == DepType::PRIVATE)
    {
        // A static library or object library can't have Dependency::PRIVATE deps, it can only have
        // Dependency::INTERFACE. But, the following publicDeps is done for correct-ordering when static-libs are
        // finally supplied to dynamic-lib or exe. Static library ignores the deps.
        ploat->publicDeps(dsc.getPLOAT(), std::move(prebuiltDep));
    }
    else
    {
        ploat->deps(depType, dsc.getPLOAT(), std::move(prebuiltDep));
    }

    assignObjectFileProducerDeps(depType, dsc);

    if constexpr (sizeof...(args))
    {
        return assignLOATDep(depType, args...);
    }
}

template <typename T>
template <typename U, typename... V>
void DSC<T>::assignLOATDep(DepType depType, DSC<U> &dsc, V... args)
{
    assignLOATDep(depType, dsc, prebuiltDepLocal);

    if constexpr (sizeof...(args))
    {
        return assignLOATDep(depType, args...);
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

template <typename T> PLOAT &DSC<T>::getPLOAT() const
{
    return *ploat;
}

template <typename T> LOAT &DSC<T>::getLOAT()
{
    return static_cast<LOAT &>(*ploat);
}

#endif // HMAKE_DSC_HPP
