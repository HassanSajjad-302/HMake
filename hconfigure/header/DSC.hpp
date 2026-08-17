#ifndef HMAKE_DSC_HPP
#define HMAKE_DSC_HPP

#include "DepType.hpp"
#include "Features.hpp"
#include "LOAT.hpp"
#include "ObjectFileProducer.hpp"

// Optional target-specific additions to DSC. A frontend can specialize this without duplicating DSC itself.
template <typename T, typename Derived> struct DSCExtension
{
    Derived &publicDeps(string_view) = delete;
    Derived &privateDeps(string_view) = delete;
    Derived &interfaceDeps(string_view) = delete;
    Derived &publicOpDeps(string_view) = delete;
    Derived &privateOpDeps(string_view) = delete;
    Derived &interfaceOpDeps(string_view) = delete;
    Derived &publicLinkDeps(string_view) = delete;
    Derived &privateLinkDeps(string_view) = delete;
    Derived &interfaceLinkDeps(string_view) = delete;
};

/// Dependency Specification Controller. Direct dependency semantics are centralized in deps().
template <typename T> struct DSC : DSCFeatures, DSCExtension<T, DSC<T>>
{
    using DSCExtension<T, DSC<T>>::interfaceDeps;
    using DSCExtension<T, DSC<T>>::interfaceLinkDeps;
    using DSCExtension<T, DSC<T>>::interfaceOpDeps;
    using DSCExtension<T, DSC<T>>::privateDeps;
    using DSCExtension<T, DSC<T>>::privateLinkDeps;
    using DSCExtension<T, DSC<T>>::privateOpDeps;
    using DSCExtension<T, DSC<T>>::publicDeps;
    using DSCExtension<T, DSC<T>>::publicLinkDeps;
    using DSCExtension<T, DSC<T>>::publicOpDeps;

    T *stored = nullptr;
    T *objectFileProducer = nullptr;
    PLOAT *ploat = nullptr;

    DSC &save(T &ptr);
    DSC &saveAndReplace(T &ptr);
    DSC &restore();

    string define;

    DSC(T *ptr, PLOAT *ploat_, bool defines = false, string define_ = "");

    template <typename U, typename... V> DSC &publicDeps(DSC<U> &dependency, V... dependencies);
    template <typename U, typename... V> DSC &privateDeps(DSC<U> &dependency, V... dependencies);
    template <typename U, typename... V> DSC &interfaceDeps(DSC<U> &dependency, V... dependencies);
    template <bool addBTargetDependency = true, typename U, typename... V>
    DSC &deps(DepType depType, bool opDependency, bool linkDependency, DSC<U> &dependency, V... dependencies);

    template <typename U, typename... V> DSC &publicOpDeps(DSC<U> &dependency, V... dependencies);
    template <typename U, typename... V> DSC &privateOpDeps(DSC<U> &dependency, V... dependencies);
    template <typename U, typename... V> DSC &interfaceOpDeps(DSC<U> &dependency, V... dependencies);
    template <typename U, typename... V> DSC &opDeps(DepType depType, DSC<U> &dependency, V... dependencies);
    DSC &publicOpDeps(ObjectFileProducer &dependency);
    DSC &privateOpDeps(ObjectFileProducer &dependency);
    DSC &interfaceOpDeps(ObjectFileProducer &dependency);
    DSC &opDeps(DepType depType, ObjectFileProducer &dependency);

    template <typename U, typename... V> DSC &publicLinkDeps(DSC<U> &dependency, V... dependencies);
    template <typename U, typename... V> DSC &privateLinkDeps(DSC<U> &dependency, V... dependencies);
    template <typename U, typename... V> DSC &interfaceLinkDeps(DSC<U> &dependency, V... dependencies);
    template <typename U, typename... V> DSC &linkDeps(DepType depType, DSC<U> &dependency, V... dependencies);
    DSC &publicLinkDeps(PLOAT &dependency);
    DSC &privateLinkDeps(PLOAT &dependency);
    DSC &interfaceLinkDeps(PLOAT &dependency);
    DSC &linkDeps(DepType depType, PLOAT &dependency);
    DSC &publicLinkDeps(ObjectFileProducer &dependency);
    DSC &privateLinkDeps(ObjectFileProducer &dependency);
    DSC &interfaceLinkDeps(ObjectFileProducer &dependency);
    DSC &linkDeps(DepType depType, ObjectFileProducer &dependency);

    T &getSourceTarget();
    T *getSourceTargetPointer();
    PLOAT &getPLOAT() const;
    LOAT &getLOAT();
};

template <typename T> bool operator<(const DSC<T> &lhs, const DSC<T> &rhs)
{
    return std::tie(lhs.objectFileProducer, lhs.ploat) < std::tie(rhs.objectFileProducer, rhs.ploat);
}

template <typename T>
DSC<T>::DSC(T *ptr, PLOAT *ploat_, const bool defines, string define_) : objectFileProducer(ptr), ploat(ploat_)
{
    if (objectFileProducer && ploat)
    {
        if (!ploat->rootObjectFileProducers.emplace(objectFileProducer).second)
        {
            printErrorMessage(FORMAT("An object-file producer was registered with a link target more than once.\n"
                                     "Link target: {}\nProducer: {}",
                                     ploat->getPrintName(), objectFileProducer->getPrintName()));
        }

        // PLOAT decides its round-zero object dependencies after producer round one has finalized hasObjectFiles.
        ploat->realBTargets[1].addDep<BTargetType::UNKNOWN>(&objectFileProducer->realBTargets[1]);
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

template <typename T> template <typename U, typename... V> DSC<T> &DSC<T>::publicDeps(DSC<U> &dependency, V... rest)
{
    return deps(DepType::PUBLIC, true, true, dependency, rest...);
}

template <typename T> template <typename U, typename... V> DSC<T> &DSC<T>::privateDeps(DSC<U> &dependency, V... rest)
{
    return deps(DepType::PRIVATE, true, true, dependency, rest...);
}

template <typename T> template <typename U, typename... V> DSC<T> &DSC<T>::interfaceDeps(DSC<U> &dependency, V... rest)
{
    return deps(DepType::INTERFACE, true, true, dependency, rest...);
}

template <typename T>
template <bool addBTargetDependency, typename U, typename... V>
DSC<T> &DSC<T>::deps(const DepType depType, const bool needsOpDependency, const bool needsLinkDependency,
                     DSC<U> &dependency, V... rest)
{
    ObjectFileProducer *dependencyProducer = dependency.objectFileProducer;
    PLOAT *dependencyPloat = dependency.ploat;

    // req is consumed by this target; useReq is inherited by its consumers:
    // PUBLIC -> req + useReq, PRIVATE -> req only, INTERFACE -> useReq only.
    const bool isReq = depType != DepType::INTERFACE;
    const bool isUseReq = depType != DepType::PRIVATE;

    // A dependency with a PLOAT supplies its binary through the PLOAT relation below. Without one, LINK means that
    // the dependency producer's raw object files must travel through the ObjectFileProducer relation instead.
    const bool rawObjectLinkDependency = needsLinkDependency && !dependencyPloat;

    if (needsOpDependency || rawObjectLinkDependency)
    {
        if (!objectFileProducer || !dependencyProducer)
        {
            printErrorMessage(needsOpDependency
                                  ? "An op dependency requires ObjectFileProducer targets on both sides."
                                  : "A raw-object link dependency requires ObjectFileProducer targets on both sides.");
        }
        if (objectFileProducer == dependencyProducer)
        {
            printErrorMessage(FORMAT("An object-file producer cannot depend on itself.\nProducer: {}",
                                     objectFileProducer->getPrintName()));
        }

        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            // These maps are flattened and cached at configuration time. Build mode consumes the cached req closure.
            // Distinct facets may merge; an unchanged merge means the same direct relation was declared twice.
            const auto addProducerDependency = [&](OpDepInfoMap &dependencies, const OpDepInfo dependencyType) {
                if (!ObjectFileProducer::mergeDependency(dependencies, dependencyProducer, dependencyType))
                {
                    printErrorMessage(FORMAT("An object-file producer dependency was specified more than once.\n"
                                             "Consumer: {}\nDependency: {}\nOp dependency: {}\nLink dependency: {}",
                                             objectFileProducer->getPrintName(), dependencyProducer->getPrintName(),
                                             dependencyType.isOpDependency(), dependencyType.isLinkDependency()));
                }
            };

            constexpr bool acyclicDependency = addBTargetDependency;
            if (isReq)
            {
                const OpDepInfo reqDependency{needsOpDependency, rawObjectLinkDependency, acyclicDependency};
                addProducerDependency(objectFileProducer->reqObjectFileProducers, reqDependency);
            }

            const bool objectsAreAbsorbedHere = ploat && ploat->bTargetType == BTargetType::LOAT;
            // PRIVATE hides compile usage, but raw objects must remain exported until a LOAT absorbs them.
            const OpDepInfo useReqDependency{needsOpDependency && isUseReq,
                                             rawObjectLinkDependency && (isUseReq || !objectsAreAbsorbedHere),
                                             acyclicDependency};
            if (useReqDependency.isOpDependency() || useReqDependency.isLinkDependency())
            {
                addProducerDependency(objectFileProducer->useReqObjectFileProducers, useReqDependency);
            }
        }

        if constexpr (addBTargetDependency)
        {
            // Both modes need the producer's round-one metadata before this target consumes its dependency closure.
            objectFileProducer->realBTargets[1].template addDep<BTargetType::UNKNOWN>(
                &dependencyProducer->realBTargets[1]);
        }
    }

    if (needsLinkDependency && dependencyPloat)
    {
        if (!ploat && !objectFileProducer)
        {
            printErrorMessage(FORMAT("A PLOAT-backed link dependency requires either a PLOAT or an "
                                     "ObjectFileProducer on the consumer.\nDependency: {}",
                                     dependencyPloat->getPrintName()));
        }
        if (ploat && ploat == dependencyPloat)
        {
            printErrorMessage(FORMAT("A link target cannot depend on itself.\nTarget: {}", ploat->getPrintName()));
        }

        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            const PloatDepInfo dependencyInfo{addBTargetDependency};
            if (ploat)
            {
                // A physical link boundary can retain the relation directly. PLOAT flattens it through useReqDeps
                // before writing the final required-library closure to its configuration cache.
                if (isReq)
                {
                    mergePloatDependency(ploat->reqDeps, dependencyPloat, dependencyInfo);
                }

                // PRIVATE is hidden only by a generated shared library. Static archives do not resolve linked-library
                // dependencies themselves, so they must continue exporting those requirements.
                if (isUseReq || ploat->linkTargetType != TargetType::LIBRARY_SHARED)
                {
                    mergePloatDependency(ploat->useReqDeps, dependencyPloat, dependencyInfo);
                }
            }
            else
            {
                // An object-only target has no output at which a PRIVATE link requirement can be absorbed. Carry it
                // through the producer graph until an eventual PLOAT consumes this producer's object files.
                if (isReq)
                {
                    mergePloatDependency(objectFileProducer->reqPloatDeps, dependencyPloat, dependencyInfo);
                }
                mergePloatDependency(objectFileProducer->useReqPloatDeps, dependencyPloat, dependencyInfo);
            }
        }

        if constexpr (addBTargetDependency)
        {
            // This edge is required in both modes: the direct PLOAT or deferred producer must observe the dependency's
            // finalized link interface before its own round-one completion.
            RealBTarget &consumerRoundOne = ploat ? ploat->realBTargets[1] : objectFileProducer->realBTargets[1];
            consumerRoundOne.addDep<BTargetType::UNKNOWN>(&dependencyPloat->realBTargets[1]);
        }
    }

    // An OP relation also propagates the dependency's DLL import definition according to the same visibility.
    if constexpr (requires(T *target) {
                      target->configuration;
                      target->reqCompileDefinitions;
                      target->useReqCompileDefinitions;
                  })
    {
        if (needsOpDependency && dependency.defineDllInterface == DefineDLLInterface::YES)
        {
            Define importedDefine(dependency.define);
            if (dependencyPloat && dependencyPloat->evaluate(TargetType::LIBRARY_SHARED) &&
                objectFileProducer->configuration->compilerFeatures.compiler.bTFamily == BTFamily::MSVC)
            {
                importedDefine.value = "__declspec(dllimport)";
            }

            if (isReq)
            {
                objectFileProducer->reqCompileDefinitions.emplace(importedDefine);
            }
            if (isUseReq)
            {
                objectFileProducer->useReqCompileDefinitions.emplace(importedDefine);
            }
        }
    }

    // Preserve declaration order while applying the same relation to every remaining DSC.
    (deps<addBTargetDependency>(depType, needsOpDependency, needsLinkDependency, rest), ...);
    return *this;
}

template <typename T> template <typename U, typename... V> DSC<T> &DSC<T>::publicOpDeps(DSC<U> &dependency, V... rest)
{
    return opDeps(DepType::PUBLIC, dependency, rest...);
}

template <typename T> template <typename U, typename... V> DSC<T> &DSC<T>::privateOpDeps(DSC<U> &dependency, V... rest)
{
    return opDeps(DepType::PRIVATE, dependency, rest...);
}

template <typename T>
template <typename U, typename... V>
DSC<T> &DSC<T>::interfaceOpDeps(DSC<U> &dependency, V... rest)
{
    return opDeps(DepType::INTERFACE, dependency, rest...);
}

template <typename T>
template <typename U, typename... V>
DSC<T> &DSC<T>::opDeps(const DepType depType, DSC<U> &dependency, V... rest)
{
    return deps(depType, true, false, dependency, rest...);
}

template <typename T> DSC<T> &DSC<T>::publicOpDeps(ObjectFileProducer &dependency)
{
    return opDeps(DepType::PUBLIC, dependency);
}

template <typename T> DSC<T> &DSC<T>::privateOpDeps(ObjectFileProducer &dependency)
{
    return opDeps(DepType::PRIVATE, dependency);
}

template <typename T> DSC<T> &DSC<T>::interfaceOpDeps(ObjectFileProducer &dependency)
{
    return opDeps(DepType::INTERFACE, dependency);
}

template <typename T> DSC<T> &DSC<T>::opDeps(const DepType depType, ObjectFileProducer &dependency)
{
    DSC<ObjectFileProducer> dependencyDsc(&dependency, nullptr);
    return deps(depType, true, false, dependencyDsc);
}

template <typename T> template <typename U, typename... V> DSC<T> &DSC<T>::publicLinkDeps(DSC<U> &dependency, V... rest)
{
    return linkDeps(DepType::PUBLIC, dependency, rest...);
}

template <typename T>
template <typename U, typename... V>
DSC<T> &DSC<T>::privateLinkDeps(DSC<U> &dependency, V... rest)
{
    return linkDeps(DepType::PRIVATE, dependency, rest...);
}

template <typename T>
template <typename U, typename... V>
DSC<T> &DSC<T>::interfaceLinkDeps(DSC<U> &dependency, V... rest)
{
    return linkDeps(DepType::INTERFACE, dependency, rest...);
}

template <typename T>
template <typename U, typename... V>
DSC<T> &DSC<T>::linkDeps(const DepType depType, DSC<U> &dependency, V... rest)
{
    return deps(depType, false, true, dependency, rest...);
}

template <typename T> DSC<T> &DSC<T>::publicLinkDeps(PLOAT &dependency)
{
    return linkDeps(DepType::PUBLIC, dependency);
}

template <typename T> DSC<T> &DSC<T>::privateLinkDeps(PLOAT &dependency)
{
    return linkDeps(DepType::PRIVATE, dependency);
}

template <typename T> DSC<T> &DSC<T>::interfaceLinkDeps(PLOAT &dependency)
{
    return linkDeps(DepType::INTERFACE, dependency);
}

template <typename T> DSC<T> &DSC<T>::linkDeps(const DepType depType, PLOAT &dependency)
{
    DSC<ObjectFileProducer> dependencyDsc(nullptr, &dependency);
    return deps(depType, false, true, dependencyDsc);
}

template <typename T> DSC<T> &DSC<T>::publicLinkDeps(ObjectFileProducer &dependency)
{
    return linkDeps(DepType::PUBLIC, dependency);
}

template <typename T> DSC<T> &DSC<T>::privateLinkDeps(ObjectFileProducer &dependency)
{
    return linkDeps(DepType::PRIVATE, dependency);
}

template <typename T> DSC<T> &DSC<T>::interfaceLinkDeps(ObjectFileProducer &dependency)
{
    return linkDeps(DepType::INTERFACE, dependency);
}

template <typename T> DSC<T> &DSC<T>::linkDeps(const DepType depType, ObjectFileProducer &dependency)
{
    DSC<ObjectFileProducer> dependencyDsc(&dependency, nullptr);
    return deps(depType, false, true, dependencyDsc);
}

template <typename T> T &DSC<T>::getSourceTarget()
{
    return *objectFileProducer;
}

template <typename T> T *DSC<T>::getSourceTargetPointer()
{
    return objectFileProducer;
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
