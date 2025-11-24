
#ifndef HMAKE_OBJECTFILEPRODUCER_HPP
#define HMAKE_OBJECTFILEPRODUCER_HPP

#include "BuildSystemFunctions.hpp"
#include "DepType.hpp"
#include "ObjectFile.hpp"

class LOAT;

class ObjectFileProducer : public BTarget
{
  public:
    bool hasObjectFiles = true;
    ObjectFileProducer()
    {
    }

    ObjectFileProducer(string name_, const bool buildExplicit, const bool makeDirectory)
        : BTarget(std::move(name_), buildExplicit, makeDirectory)
    {
    }
    virtual void getObjectFiles(vector<const ObjectFile *> *objectFiles) const
    {
    }
};

// Dependency Specification CRTP
template <typename T> struct ObjectFileProducerWithDS : ObjectFileProducer
{
    ObjectFileProducerWithDS();
    ObjectFileProducerWithDS(string name_, bool buildExplicit, bool makeDirectory);

    // Following 2 unused at BSMode::Build
    // we need this to be ordered in setCompileCommand. order is deterministic as insertions are supposed to be always
    // in order
    btree_set<T *, TPointerLess<T>> reqDeps;
    flat_hash_set<T *> useReqDeps;

    template <typename... U> T &publicDeps(T &objectFileProducer, U &&...objectFileProducers);
    template <typename... U> T &privateDeps(T &objectFileProducer, U &&...objectFileProducers);
    template <typename... U> T &interfaceDeps(T &objectFileProducer, U &&...objectFileProducers);

    template <typename... U> T &deps(DepType depType, T &objectFileProducer, U &&...objectFileProducers);

    void populateReqAndUseReqDeps();
};

template <typename T> ObjectFileProducerWithDS<T>::ObjectFileProducerWithDS() = default;

template <typename T>
ObjectFileProducerWithDS<T>::ObjectFileProducerWithDS(string name_, const bool buildExplicit, const bool makeDirectory)
    : ObjectFileProducer(std::move(name_), buildExplicit, makeDirectory)
{
}

template <typename T>
template <typename... U>
T &ObjectFileProducerWithDS<T>::publicDeps(T &objectFileProducer, U &&...objectFileProducers)
{
    deps(DepType::PUBLIC, objectFileProducer);
    if constexpr (sizeof...(objectFileProducers))
    {
        return publicDeps(objectFileProducers...);
    }
    return static_cast<T &>(*this);
}

template <typename T>
template <typename... U>
T &ObjectFileProducerWithDS<T>::privateDeps(T &objectFileProducer, U &&...objectFileProducers)
{
    deps(DepType::PRIVATE, objectFileProducer);
    if constexpr (sizeof...(objectFileProducers))
    {
        return privateDeps(objectFileProducers...);
    }
    return static_cast<T &>(*this);
}

template <typename T>
template <typename... U>
T &ObjectFileProducerWithDS<T>::interfaceDeps(T &objectFileProducer, U &&...objectFileProducers)
{
    deps(DepType::INTERFACE, objectFileProducer);
    if constexpr (sizeof...(objectFileProducers))
    {
        return interfaceDeps(objectFileProducers...);
    }
    return static_cast<T &>(*this);
}

template <typename T>
template <typename... U>
T &ObjectFileProducerWithDS<T>::deps(const DepType depType, T &objectFileProducer, U &&...objectFileProducers)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        TargetCache *us = static_cast<TargetCache *>(static_cast<T *>(this));
        TargetCache *ourDep = static_cast<TargetCache *>(&objectFileProducer);
        if (ourDep->cacheIndex > us->cacheIndex)
        {
            printErrorMessage(FORMAT("Please declare dependency \n{}\n before its dependent \n{}\nDependency "
                                     "declaration before the dependent is an invariant in HMake.",
                                     fileTargetCaches[ourDep->cacheIndex].name, fileTargetCaches[us->cacheIndex].name));
        }

        if (depType == DepType::PUBLIC)
        {
            reqDeps.emplace(&objectFileProducer);
            useReqDeps.emplace(&objectFileProducer);
            addDepNow<1>(objectFileProducer);
        }
        else if (depType == DepType::PRIVATE)
        {
            reqDeps.emplace(&objectFileProducer);
            addDepNow<1>(objectFileProducer);
        }
        else
        {
            useReqDeps.emplace(&objectFileProducer);
        }
    }
    else
    {
        addDepNow<0, BTargetDepType::SELECTIVE>(objectFileProducer);
    }

    if constexpr (sizeof...(objectFileProducers))
    {
        return deps(objectFileProducers...);
    }
    return static_cast<T &>(*this);
}

template <typename T> void ObjectFileProducerWithDS<T>::populateReqAndUseReqDeps()
{
    // Set is copied because new elements are to be inserted in it.

    for (auto localReqDeps = reqDeps; T * t : localReqDeps)
    {
        for (T *t_ : t->useReqDeps)
        {
            reqDeps.emplace(t_);
        }
    }

    for (auto localUseReqDeps = useReqDeps; T * t : localUseReqDeps)
    {
        for (T *t_ : t->useReqDeps)
        {
            useReqDeps.emplace(t_);
        }
    }
}

#endif // HMAKE_OBJECTFILEPRODUCER_HPP
