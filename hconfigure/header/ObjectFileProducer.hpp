
#ifndef HMAKE_OBJECTFILEPRODUCER_HPP
#define HMAKE_OBJECTFILEPRODUCER_HPP
#ifdef USE_HEADER_UNITS
import "Dependency.hpp";
import "LinkOrArchiveTarget.hpp";
import "ObjectFile.hpp";
#else
#include "Dependency.hpp"
#include "LinkOrArchiveTarget.hpp"
#include "ObjectFile.hpp"
#endif

class ObjectFileProducer : public BTarget
{
  public:
    ObjectFileProducer();
    ObjectFileProducer(string name_, bool buildExplicit, bool makeDirectory);
    virtual void getObjectFiles(vector<const ObjectFile *> *objectFiles,
                                LinkOrArchiveTarget *linkOrArchiveTarget) const;
};

// Dependency Specification CRTP
template <typename T> struct ObjectFileProducerWithDS : ObjectFileProducer
{
    ObjectFileProducerWithDS();
    ObjectFileProducerWithDS(string name_, bool buildExplicit, bool makeDirectory);
    flat_hash_set<T *> requirementDeps;
    flat_hash_set<T *> usageRequirementDeps;
    template <typename... U> T &publicDeps(T *dep, const U... deps);
    template <typename... U> T &privateDeps(T *dep, const U... deps);
    template <typename... U> T &interfaceDeps(T *dep, const U... deps);

    template <typename... U> T &deps(T *dep, Dependency dependency, const U... deps);

    void populateRequirementAndUsageRequirementDeps();
};

template <typename T> ObjectFileProducerWithDS<T>::ObjectFileProducerWithDS() = default;

template <typename T>
ObjectFileProducerWithDS<T>::ObjectFileProducerWithDS(string name_, bool buildExplicit, bool makeDirectory)
    : ObjectFileProducer(std::move(name_), buildExplicit, makeDirectory)
{
}

template <typename T> template <typename... U> T &ObjectFileProducerWithDS<T>::publicDeps(T *dep, const U... deps)
{
    requirementDeps.emplace(dep);
    usageRequirementDeps.emplace(dep);
    realBTargets[2].addDependency(*dep);
    if constexpr (sizeof...(deps))
    {
        return publicDeps(deps...);
    }
    return static_cast<T &>(*this);
}

template <typename T> template <typename... U> T &ObjectFileProducerWithDS<T>::privateDeps(T *dep, const U... deps)
{
    requirementDeps.emplace(dep);
    realBTargets[2].addDependency(*dep);
    if constexpr (sizeof...(deps))
    {
        return privateDeps(deps...);
    }
    return static_cast<T &>(*this);
}

template <typename T> template <typename... U> T &ObjectFileProducerWithDS<T>::interfaceDeps(T *dep, const U... deps)
{
    usageRequirementDeps.emplace(dep);
    realBTargets[2].addDependency(*dep);
    if constexpr (sizeof...(deps))
    {
        return interfaceDeps(deps...);
    }
    return static_cast<T &>(*this);
}

template <typename T>
template <typename... U>
T &ObjectFileProducerWithDS<T>::deps(T *dep, const Dependency dependency, const U... deps)
{
    if (dependency == Dependency::PUBLIC)
    {
        requirementDeps.emplace(dep);
        usageRequirementDeps.emplace(dep);
        realBTargets[2].addDependency(*dep);
    }
    else if (dependency == Dependency::PRIVATE)
    {
        requirementDeps.emplace(dep);
        realBTargets[2].addDependency(*dep);
    }
    else
    {
        usageRequirementDeps.emplace(dep);
        realBTargets[2].addDependency(*dep);
    }
    if constexpr (sizeof...(deps))
    {
        return DEPS(deps...);
    }
    return static_cast<T &>(*this);
}

template <typename T> void ObjectFileProducerWithDS<T>::populateRequirementAndUsageRequirementDeps()
{
    // Set is copied because new elements are to be inserted in it.

    for (flat_hash_set<T *> localRequirementDeps = requirementDeps; T * t : localRequirementDeps)
    {
        for (T *t_ : t->usageRequirementDeps)
        {
            requirementDeps.emplace(t_);
        }
    }

    for (flat_hash_set<T *> localUsageRequirementDeps = usageRequirementDeps; T *t : localUsageRequirementDeps)
    {
        for (T *t_ : t->usageRequirementDeps)
        {
            usageRequirementDeps.emplace(t_);
        }
    }
}

#endif // HMAKE_OBJECTFILEPRODUCER_HPP
