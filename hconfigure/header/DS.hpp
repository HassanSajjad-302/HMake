
#ifndef HMAKE_DS_HPP
#define HMAKE_DS_HPP

#include <concepts>
#include <set>

using std::set, std::same_as;

// Dependency Specification CRTP
template <typename T> struct DS
{
    set<T *> requirementDeps;
    set<T *> usageRequirementDeps;
    template <same_as<T *>... U> T &PUBLIC_LIBRARIES(const U... libraries);
    template <same_as<T *>... U> T &PRIVATE_LIBRARIES(const U... libraries);
    template <same_as<T *>... U> T &INTERFACE_LIBRARIES(const U... libraries);
    void populateRequirementAndUsageRequirementDeps();
};

template <typename T> template <same_as<T *>... U> T &DS<T>::INTERFACE_LIBRARIES(const U... libraries)
{
    (requirementDeps.emplace(libraries...));
    (usageRequirementDeps.emplace(libraries...));
    return static_cast<T &>(*this);
}

template <typename T> template <same_as<T *>... U> T &DS<T>::PRIVATE_LIBRARIES(const U... libraries)
{
    (requirementDeps.emplace(libraries...));
    return static_cast<T &>(*this);
}

template <typename T> template <same_as<T *>... U> T &DS<T>::PUBLIC_LIBRARIES(const U... libraries)
{
    (requirementDeps.emplace(libraries...));
    (usageRequirementDeps.emplace(libraries...));
    return static_cast<T &>(*this);
}

template <typename T> void DS<T>::populateRequirementAndUsageRequirementDeps()
{
    // Set is copied because new elements are to be inserted in it.
    set<T *> localRequirementLibs = requirementDeps;

    for (T *t : localRequirementLibs)
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
