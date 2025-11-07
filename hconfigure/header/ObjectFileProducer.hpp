
#ifndef HMAKE_OBJECTFILEPRODUCER_HPP
#define HMAKE_OBJECTFILEPRODUCER_HPP

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
    virtual void getObjectFiles(vector<const ObjectFile *> *objectFiles, LOAT *loat) const
    {
    }
};

// Dependency Specification CRTP
template <typename T> struct ObjectFileProducerWithDS : ObjectFileProducer
{
    ObjectFileProducerWithDS();
    ObjectFileProducerWithDS(string name_, bool buildExplicit, bool makeDirectory);

    // Custom comparator for BTarget* based on id
    struct TPointerLess
    {
        bool operator()(const T *lhs, const T *rhs) const
        {
            // Compare based on CppTarget::cacheIndex for ordering
            return lhs->cacheIndex < rhs->cacheIndex;
        }
    };

    // Following 2 unused at BSMode::Build
    // we need this to be ordered in setCompileCommand. order is deterministic as insertions are supposed to be always
    // in order
    phmap::btree_set<T *, TPointerLess> reqDeps;
    flat_hash_set<T *> useReqDeps;

    template <typename... U> T &deps(const DepType depType, T &dep, U &&...deps);

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
T &ObjectFileProducerWithDS<T>::deps(const DepType depType, T &dep, U &&...objectFileDeps)
{
    if (depType == DepType::PUBLIC)
    {
        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            reqDeps.emplace(&dep);
            useReqDeps.emplace(&dep);
            addDepNow<1>(dep);
        }
        addDepNow<0, BTargetDepType::SELECTIVE>(dep);
    }
    else if (depType == DepType::PRIVATE)
    {
        if constexpr (bsMode == BSMode::CONFIGURE)
        {
            reqDeps.emplace(&dep);
            addDepNow<1>(dep);
        }
        addDepNow<0, BTargetDepType::SELECTIVE>(dep);
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
