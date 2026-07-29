
#ifndef HMAKE_DSC_HPP
#define HMAKE_DSC_HPP

#include "Features.hpp"
#include "LOAT.hpp"
#include "ObjectFileProducer.hpp"

// Optional target-specific additions to DSC. The primary template keeps named dependencies unavailable. A target layer
// can specialize this extension without specializing or duplicating DSC itself.
template <typename T, typename Derived> struct DSCExtension
{
    Derived &publicDeps(string_view) = delete;
    Derived &privateDeps(string_view) = delete;
    Derived &interfaceDeps(string_view) = delete;
    Derived &publicCompileDeps(string_view) = delete;
    Derived &privateCompileDeps(string_view) = delete;
    Derived &interfaceCompileDeps(string_view) = delete;
    Derived &publicLinkDeps(string_view) = delete;
    Derived &privateLinkDeps(string_view) = delete;
    Derived &interfaceLinkDeps(string_view) = delete;
};

// Dependency Specification Controller. The following declaration is for T = CSourceTarget
template <typename T> struct DSC : DSCFeatures, DSCExtension<T, DSC<T>>
{
    using DSCExtension<T, DSC<T>>::interfaceCompileDeps;
    using DSCExtension<T, DSC<T>>::interfaceDeps;
    using DSCExtension<T, DSC<T>>::interfaceLinkDeps;
    using DSCExtension<T, DSC<T>>::privateCompileDeps;
    using DSCExtension<T, DSC<T>>::privateDeps;
    using DSCExtension<T, DSC<T>>::privateLinkDeps;
    using DSCExtension<T, DSC<T>>::publicCompileDeps;
    using DSCExtension<T, DSC<T>>::publicDeps;
    using DSCExtension<T, DSC<T>>::publicLinkDeps;

    T *stored = nullptr;
    T *objectFileProducer = nullptr;
    PLOAT *ploat = nullptr;

    template <typename U> void addCompileDependency(DepType depType, DSC<U> &depDSC);
    template <typename U> void addLinkDependency(DepType depType, DSC<U> &depDSC);

    DSC &save(T &ptr);
    DSC &saveAndReplace(T &ptr);
    DSC &restore();

    string define;

    DSC(T *ptr, PLOAT *ploat_, bool defines = false, string define_ = "");

    template <typename U, typename... V> DSC &publicDeps(DSC<U> &depDSC, V... dscs);
    template <typename U, typename... V> DSC &privateDeps(DSC<U> &depDSC, V... dscs);
    template <typename U, typename... V> DSC &interfaceDeps(DSC<U> &depDSC, V... dscs);
    template <typename U, typename... V> DSC &deps(DepType depType, DSC<U> &depDSC, V... dscs);

    template <typename U, typename... V> DSC &publicCompileDeps(DSC<U> &depDSC, V... dscs);
    template <typename U, typename... V> DSC &privateCompileDeps(DSC<U> &depDSC, V... dscs);
    template <typename U, typename... V> DSC &interfaceCompileDeps(DSC<U> &depDSC, V... dscs);
    template <typename U, typename... V> DSC &compileDeps(DepType depType, DSC<U> &depDSC, V... dscs);

    template <typename U, typename... V> DSC &publicLinkDeps(DSC<U> &depDSC, V... dscs);
    template <typename U, typename... V> DSC &privateLinkDeps(DSC<U> &depDSC, V... dscs);
    template <typename U, typename... V> DSC &interfaceLinkDeps(DSC<U> &depDSC, V... dscs);
    template <typename U, typename... V> DSC &linkDeps(DepType depType, DSC<U> &depDSC, V... dscs);

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
    addLinkDependency(depType, depDSC);
    addCompileDependency(depType, depDSC);

    if constexpr (sizeof...(dscs))
    {
        return deps(depType, dscs...);
    }
    return *this;
}

template <typename T>
template <typename U>
void DSC<T>::addLinkDependency(const DepType depType, DSC<U> &depDSC)
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
    else if (ploat && !depDSC.ploat)
    {
        if (depDSC.objectFileProducer)
        {
            ploat->objectFileProducers.emplace(depDSC.objectFileProducer);
            if (objectFileProducer->hasObjectFiles)
            {
                ploat->hasObjectFiles = true;
            }
        }
    }
}

template <typename T>
template <typename U>
void DSC<T>::addCompileDependency(const DepType depType, DSC<U> &depDSC)
{
    objectFileProducer->addCompileDependency(depType, depDSC.getSourceTarget());

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
}

template <typename T>
template <typename U, typename... V>
DSC<T> &DSC<T>::publicCompileDeps(DSC<U> &depDSC, V... dscs)
{
    return compileDeps(DepType::PUBLIC, depDSC, dscs...);
}

template <typename T>
template <typename U, typename... V>
DSC<T> &DSC<T>::privateCompileDeps(DSC<U> &depDSC, V... dscs)
{
    return compileDeps(DepType::PRIVATE, depDSC, dscs...);
}

template <typename T>
template <typename U, typename... V>
DSC<T> &DSC<T>::interfaceCompileDeps(DSC<U> &depDSC, V... dscs)
{
    return compileDeps(DepType::INTERFACE, depDSC, dscs...);
}

template <typename T>
template <typename U, typename... V>
DSC<T> &DSC<T>::compileDeps(const DepType depType, DSC<U> &depDSC, V... dscs)
{
    addCompileDependency(depType, depDSC);

    if constexpr (sizeof...(dscs))
    {
        return compileDeps(depType, dscs...);
    }
    return *this;
}

template <typename T>
template <typename U, typename... V>
DSC<T> &DSC<T>::publicLinkDeps(DSC<U> &depDSC, V... dscs)
{
    return linkDeps(DepType::PUBLIC, depDSC, dscs...);
}

template <typename T>
template <typename U, typename... V>
DSC<T> &DSC<T>::privateLinkDeps(DSC<U> &depDSC, V... dscs)
{
    return linkDeps(DepType::PRIVATE, depDSC, dscs...);
}

template <typename T>
template <typename U, typename... V>
DSC<T> &DSC<T>::interfaceLinkDeps(DSC<U> &depDSC, V... dscs)
{
    return linkDeps(DepType::INTERFACE, depDSC, dscs...);
}

template <typename T>
template <typename U, typename... V>
DSC<T> &DSC<T>::linkDeps(const DepType depType, DSC<U> &depDSC, V... dscs)
{
    addLinkDependency(depType, depDSC);

    if constexpr (sizeof...(dscs))
    {
        return linkDeps(depType, dscs...);
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
