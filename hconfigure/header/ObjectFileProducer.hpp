
#ifndef HMAKE_OBJECTFILEPRODUCER_HPP
#define HMAKE_OBJECTFILEPRODUCER_HPP
#ifdef USE_HEADER_UNITS
import "BasicTargets.hpp";
import "Dependency.hpp";
import "Node.hpp";
#else
#include "BasicTargets.hpp"
#include "Dependency.hpp"
#include "Node.hpp"
#endif

class ObjectFile : public BTarget
{
  public:
    Node *objectFileOutputFilePath = nullptr;
    virtual pstring getObjectFileOutputFilePathPrint(const PathPrint &pathPrint) const = 0;
};

class ObjectFileProducer : public BTarget
{
  public:
    ObjectFileProducer();
    virtual void getObjectFiles(vector<const ObjectFile *> *objectFiles,
                                class LinkOrArchiveTarget *linkOrArchiveTarget) const;
};

// Dependency Specification CRTP
template <typename T> struct ObjectFileProducerWithDS : public ObjectFileProducer
{
    set<T *> requirementDeps;
    set<T *> usageRequirementDeps;
    template <typename... U> T &PUBLIC_DEPS(T *dep, const U... deps);
    template <typename... U> T &PRIVATE_DEPS(T *dep, const U... deps);
    template <typename... U> T &INTERFACE_DEPS(T *dep, const U... deps);

    template <typename... U> T &DEPS(T *dep, Dependency dependency, const U... deps);

    void populateRequirementAndUsageRequirementDeps();
};

template <typename T> template <typename... U> T &ObjectFileProducerWithDS<T>::INTERFACE_DEPS(T *dep, const U... deps)
{
    usageRequirementDeps.emplace(dep);
    realBTargets[2].addDependency(*dep);
    if constexpr (sizeof...(deps))
    {
        return INTERFACE_DEPS(deps...);
    }
    return static_cast<T &>(*this);
}

template <typename T> template <typename... U> T &ObjectFileProducerWithDS<T>::PRIVATE_DEPS(T *dep, const U... deps)
{
    requirementDeps.emplace(dep);
    realBTargets[2].addDependency(*dep);
    if constexpr (sizeof...(deps))
    {
        return PRIVATE_DEPS(deps...);
    }
    return static_cast<T &>(*this);
}

template <typename T> template <typename... U> T &ObjectFileProducerWithDS<T>::PUBLIC_DEPS(T *dep, const U... deps)
{
    requirementDeps.emplace(dep);
    usageRequirementDeps.emplace(dep);
    realBTargets[2].addDependency(*dep);
    if constexpr (sizeof...(deps))
    {
        return PUBLIC_DEPS(deps...);
    }
    return static_cast<T &>(*this);
}

template <typename T>
template <typename... U>
T &ObjectFileProducerWithDS<T>::DEPS(T *dep, Dependency dependency, const U... deps)
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
    set<T *> localRequirementDeps = requirementDeps;

    for (T *t : localRequirementDeps)
    {
        for (T *t_ : t->usageRequirementDeps)
        {
            requirementDeps.emplace(t_);
        }
    }

    for (T *t : usageRequirementDeps)
    {
        for (T *t_ : t->usageRequirementDeps)
        {
            usageRequirementDeps.emplace(t_);
        }
    }
}

#endif // HMAKE_OBJECTFILEPRODUCER_HPP
