
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
    template <typename... U> T &PUBLIC_DEPS(const U... libraries);
    template <typename... U> T &PRIVATE_DEPS(const U... libraries);
    template <typename... U> T &INTERFACE_DEPS(const U... libraries);

    template <typename V, typename... U> T &DEPS(V library, Dependency dependency, const U... libraries);

    void populateRequirementAndUsageRequirementDeps();
};

template <typename T> template <typename... U> T &DS<T>::INTERFACE_DEPS(const U... libraries)
{
    (usageRequirementDeps.emplace(libraries...));
    return static_cast<T &>(*this);
}

template <typename T> template <typename... U> T &DS<T>::PRIVATE_DEPS(const U... libraries)
{
    (requirementDeps.emplace(libraries...));
    return static_cast<T &>(*this);
}

template <typename T> template <typename... U> T &DS<T>::PUBLIC_DEPS(const U... libraries)
{
    (requirementDeps.emplace(libraries...));
    (usageRequirementDeps.emplace(libraries...));
    return static_cast<T &>(*this);
}

template <typename T>
template <typename V, typename... U>
T &DS<T>::DEPS(V library, Dependency dependency, const U... libraries)
{
    if (dependency == Dependency::PUBLIC)
    {
        (requirementDeps.emplace(library));
        (usageRequirementDeps.emplace(library));
    }
    else if (dependency == Dependency::PRIVATE)
    {
        (requirementDeps.emplace(library));
    }
    else
    {
        (usageRequirementDeps.emplace(library));
    }
    if constexpr (sizeof...(libraries))
    {
        return DEPS(libraries...);
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
