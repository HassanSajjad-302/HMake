
#ifndef HMAKE_DS_HPP
#define HMAKE_DS_HPP
#ifdef USE_HEADER_UNITS
import "Dependency.hpp";
import <concepts>;
import <set>;
#else
#include "Dependency.hpp"
#include <concepts>
#include <set>
#endif

using std::set, std::same_as;

// Dependency Specification CRTP
template <typename T> struct DS
{
    set<T *> requirementDeps;
    set<T *> usageRequirementDeps;
    template <typename... U> T &PUBLIC_DEPS(T *dep, const U... deps);
    template <typename... U> T &PRIVATE_DEPS(T *dep, const U... deps);
    template <typename... U> T &INTERFACE_DEPS(T *dep, const U... deps);

    template <typename... U> T &DEPS(T *dep, Dependency dependency, const U... deps);

    void populateRequirementAndUsageRequirementDeps();
};

template <typename T> template <typename... U> T &DS<T>::INTERFACE_DEPS(T *dep, const U... deps)
{
    usageRequirementDeps.emplace(dep);
    if constexpr (sizeof...(deps))
    {
        return INTERFACE_DEPS(deps...);
    }
    return static_cast<T &>(*this);
}

template <typename T> template <typename... U> T &DS<T>::PRIVATE_DEPS(T *dep, const U... deps)
{
    requirementDeps.emplace(dep);
    if constexpr (sizeof...(deps))
    {
        return PRIVATE_DEPS(deps...);
    }
    return static_cast<T &>(*this);
}

template <typename T> template <typename... U> T &DS<T>::PUBLIC_DEPS(T *dep, const U... deps)
{
    requirementDeps.emplace(dep);
    usageRequirementDeps.emplace(dep);
    if constexpr (sizeof...(deps))
    {
        return PUBLIC_DEPS(deps...);
    }
    return static_cast<T &>(*this);
}

template <typename T> template <typename... U> T &DS<T>::DEPS(T *dep, Dependency dependency, const U... deps)
{
    if (dependency == Dependency::PUBLIC)
    {
        requirementDeps.emplace(dep);
        usageRequirementDeps.emplace(dep);
    }
    else if (dependency == Dependency::PRIVATE)
    {
        requirementDeps.emplace(dep);
    }
    else
    {
        usageRequirementDeps.emplace(dep);
    }
    if constexpr (sizeof...(deps))
    {
        return DEPS(deps...);
    }
    return static_cast<T &>(*this);
}

template <typename T> void DS<T>::populateRequirementAndUsageRequirementDeps()
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

#endif // HMAKE_DS_HPP
