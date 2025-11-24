
#ifndef HMAKE_DSC_HPP
#define HMAKE_DSC_HPP

#include "Features.hpp"
#include "LOAT.hpp"
#include "ObjectFileProducer.hpp"

// Dependency Specification Controller. The following declaration is for T = CSourceTarget
template <typename T> struct DSC : DSCFeatures
{
    T *stored = nullptr;
    ObjectFileProducerWithDS<T> *objectFileProducer = nullptr;
    PLOAT *ploat = nullptr;

    template <typename U> void assignObjectFileProducerDeps(DepType depType, DSC<U> &dsc);

    DSC &save(T &ptr);
    DSC &saveAndReplace(T &ptr);
    DSC &restore();

    string define;

    DSC(T *ptr, PLOAT *ploat_, bool defines = false, string define_ = "");

    template <typename U, typename... V> DSC &publicDeps(DSC<U> &depDSC, V... dscs);
    template <typename U, typename... V> DSC &privateDeps(DSC<U> &depDSC, V... dscs);
    template <typename U, typename... V> DSC &interfaceDeps(DSC<U> &depDSC, V... dscs);
    template <typename U, typename... V> DSC &deps(DepType depType, DSC<U> &depDSC, V... dscs);

    T &getSourceTarget();
    T *getSourceTargetPointer();
    PLOAT &getPLOAT() const;
    LOAT &getLOAT();
};

template <typename T> bool operator<(const DSC<T> &lhs, const DSC<T> &rhs)
{
    return std::tie(lhs.objectFileProducer, lhs.ploat) < std::tie(rhs.objectFileProducer, rhs.ploat);
}

template <typename T> DSC<T>::DSC(T *ptr, PLOAT *ploat_, bool defines, string define_)
{
    objectFileProducer = ptr;
    ploat = ploat_;
    if (ploat)
    {
        ploat->objectFileProducers.emplace(objectFileProducer);
        if (objectFileProducer->hasObjectFiles)
        {
            ploat->hasObjectFiles = true;
        }
    }

    if (define_.empty() && ploat)
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

template <typename T> template <typename U, typename... V> DSC<T> &DSC<T>::publicDeps(DSC<U> &depDSC, V... dscs)
{
    deps(DepType::PUBLIC, depDSC, dscs...);
    return *this;
}

template <typename T> template <typename U, typename... V> DSC<T> &DSC<T>::privateDeps(DSC<U> &depDSC, V... dscs)
{
    deps(DepType::PRIVATE, depDSC, dscs...);
    return *this;
}

template <typename T> template <typename U, typename... V> DSC<T> &DSC<T>::interfaceDeps(DSC<U> &depDSC, V... dscs)
{
    deps(DepType::INTERFACE, depDSC, dscs...);
    return *this;
}

template <typename T>
template <typename U, typename... V>
DSC<T> &DSC<T>::deps(DepType depType, DSC<U> &depDSC, V... dscs)
{
    if (ploat && depDSC.ploat)
    {
        if (ploat->linkTargetType != TargetType::LIBRARY_SHARED && depType == DepType::PRIVATE)
        {
            // A static library or object library can't have Dependency::PRIVATE deps, it can only have
            // Dependency::INTERFACE. But, the following publicDeps is done for correct-ordering when static-libs are
            // finally supplied to dynamic-lib or exe. Static library ignores the deps.
            ploat->publicDeps(depDSC.getPLOAT());
        }
        else
        {
            ploat->deps(depType, depDSC.getPLOAT());
        }
    }

    objectFileProducer->deps(depType, depDSC.getSourceTarget());

    if (depDSC.defineDllInterface == DefineDLLInterface::YES)
    {
        T *ptr = static_cast<T *>(objectFileProducer);

        Define define;
        define.name = depDSC.define;

        if (depDSC.ploat && depDSC.ploat->evaluate(TargetType::LIBRARY_SHARED))
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

    if constexpr (sizeof...(dscs))
    {
        return deps(depType, dscs...);
    }

    if constexpr (sizeof...(dscs))
    {
        return deps(depType, dscs...);
    }
    return *this;
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
