
#ifndef HMAKE_OBJECTFILEPRODUCER_HPP
#define HMAKE_OBJECTFILEPRODUCER_HPP
#ifdef USE_HEADER_UNITS
import "DepType.hpp";
import "LOAT.hpp";
import "ObjectFile.hpp";
#else
#include "DepType.hpp"
#include "LOAT.hpp"
#include "ObjectFile.hpp"
#endif

class ObjectFileProducer : public BTarget
{
  public:
    ObjectFileProducer();
    ObjectFileProducer(string name_, bool buildExplicit, bool makeDirectory);
    virtual void getObjectFiles(vector<const ObjectFile *> *objectFiles, LOAT *loat) const;
};

// Dependency Specification CRTP
template <typename T> struct ObjectFileProducerWithDS : ObjectFileProducer
{
    ObjectFileProducerWithDS();
    ObjectFileProducerWithDS(string name_, bool buildExplicit, bool makeDirectory);
    flat_hash_set<T *> reqDeps;
    flat_hash_set<T *> useReqDeps;
    template <typename... U> T &publicDeps(T &dep, U &&...deps);
    template <typename... U> T &privateDeps(T &dep, U &&...deps);
    template <typename... U> T &interfaceDeps(T &dep, U &&...deps);

    template <typename... U> T &deps(const DepType depType, T &dep, U &&...deps);

    void populateReqAndUseReqDeps();
};

template <typename T> ObjectFileProducerWithDS<T>::ObjectFileProducerWithDS() = default;

template <typename T>
ObjectFileProducerWithDS<T>::ObjectFileProducerWithDS(string name_, const bool buildExplicit, const bool makeDirectory)
    : ObjectFileProducer(std::move(name_), buildExplicit, makeDirectory)
{
}

template <typename T> template <typename... U> T &ObjectFileProducerWithDS<T>::publicDeps(T &dep, U &&...deps)
{
    reqDeps.emplace(&dep);
    useReqDeps.emplace(&dep);
    addDependency<2>(dep);
    if constexpr (sizeof...(deps))
    {
        return publicDeps(deps...);
    }
    return static_cast<T &>(*this);
}

template <typename T> template <typename... U> T &ObjectFileProducerWithDS<T>::privateDeps(T &dep, U &&...deps)
{
    reqDeps.emplace(&dep);
    addDependency<2>(*dep);
    if constexpr (sizeof...(deps))
    {
        return privateDeps(deps...);
    }
    return static_cast<T &>(*this);
}

template <typename T> template <typename... U> T &ObjectFileProducerWithDS<T>::interfaceDeps(T &dep, U &&...deps)
{
    useReqDeps.emplace(&dep);
    if constexpr (sizeof...(deps))
    {
        return interfaceDeps(deps...);
    }
    return static_cast<T &>(*this);
}

template <typename T>
template <typename... U>
T &ObjectFileProducerWithDS<T>::deps(const DepType depType, T &dep, U &&...objectFileDeps)
{
    if (depType == DepType::PUBLIC)
    {
        reqDeps.emplace(&dep);
        useReqDeps.emplace(&dep);
        addDepNow<2>(dep);
    }
    else if (depType == DepType::PRIVATE)
    {
        reqDeps.emplace(&dep);
        addDepNow<2>(dep);
    }
    else
    {
        useReqDeps.emplace(&dep);
    }
    if constexpr (sizeof...(objectFileDeps))
    {
        return deps(objectFileDeps...);
    }
    return static_cast<T &>(*this);
}

template <typename T> void ObjectFileProducerWithDS<T>::populateReqAndUseReqDeps()
{
    // Set is copied because new elements are to be inserted in it.

    for (flat_hash_set<T *> localReqDeps = reqDeps; T * t : localReqDeps)
    {
        for (T *t_ : t->useReqDeps)
        {
            reqDeps.emplace(t_);
        }
    }

    for (flat_hash_set<T *> localUseReqDeps = useReqDeps; T * t : localUseReqDeps)
    {
        for (T *t_ : t->useReqDeps)
        {
            useReqDeps.emplace(t_);
        }
    }
}

#endif // HMAKE_OBJECTFILEPRODUCER_HPP
