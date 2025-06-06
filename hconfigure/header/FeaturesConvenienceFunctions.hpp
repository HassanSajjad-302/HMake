#ifndef HMAKE_FEATURESCONVENIENCEFUNCTIONS_HPP
#define HMAKE_FEATURESCONVENIENCEFUNCTIONS_HPP
#ifdef USE_HEADER_UNITS
import "DepType.hpp";
#else
#include "DepType.hpp"
#endif

// TODO
// Write a concept that checks for the presence of evaluate() and assign() functions.
// CRTP Pattern Used for defining Convenience Features that are used with CppCompilerFeatures and LinkerFeatures.
// evaluate() and assign() are defined in the derived class.
template <typename U> class FeatureConvenienceFunctions
{
  protected:
    // returns on first positive condition. space is added.
    template <typename T, typename... Argument>
    string GET_FLAG_evaluate(T condition, const string &flags, Argument... arguments) const;

    // returns cumulated string for trued conditions. spaces are added
    template <typename T, typename... Argument>
    string GET_CUMULATED_FLAG_evaluate(T condition, const string &flags, Argument... arguments) const;

  protected:
    // var left = right;
    // Only last property will be assigned. Others aren't considered.
    template <DepType dependency = DepType::PRIVATE, typename T, typename... Condition>
    void RIGHT_MOST(T condition, Condition... conditions);

  public:
    // Properties are assigned if assign bool is true.
    template <DepType dependency = DepType::PRIVATE, typename T, typename... Condition>
    U &assign(bool assign, T property, Condition... conditions);
#define assign_I assign<Dependency::INTERFACE>
    template <typename T, typename... Condition> bool AND(T condition, Condition... conditions) const;
    template <typename T, typename... Condition> bool OR(T condition, Condition... conditions) const;

    // Multiple properties on left which are anded and after that right's assignment occurs.
    template <DepType dependency = DepType::PRIVATE, typename T, typename... Condition>
    U &M_LEFT_AND(T condition, Condition... conditions);
#define M_LEFT_AND_I M_LEFT_AND<Dependency::INTERFACE>

    // Single Condition and Property. M_LEFT_AND or M_LEFT_OR could be used too, but, that's more clear
    template <DepType dependency = DepType::PRIVATE, typename T, typename P> U &SINGLE(T condition, P property);
#define SINGLE_I SINGLE<Dependency::INTERFACE>

    // Multiple properties on left which are orred and after that right's assignment occurs.
    template <DepType dependency = DepType::PRIVATE, typename T, typename... Condition>
    U &M_LEFT_OR(T condition, Condition... conditions);
#define M_LEFT_OR_I M_LEFT_OR<Dependency::INTERFACE>

    // Incomplete
    // If first left succeeds then, Multiple properties of the right are assigned to the target.
    template <DepType dependency = DepType::PRIVATE, typename T, typename... Condition>
    U &M_RIGHT(T condition, Condition... conditions);
#define M_RIGHT_I M_RIGHT<Dependency::INTERFACE>
};

template <typename U>
template <typename T, typename... Argument>
string FeatureConvenienceFunctions<U>::GET_FLAG_evaluate(T condition, const string &flags,
                                                          Argument... arguments) const
{
    if (static_cast<const U &>(*this).evaluate(condition))
    {
        return flags;
    }
    if constexpr (sizeof...(arguments))
    {
        return GET_FLAG_evaluate(arguments...);
    }
    else
    {
        return "";
    }
}

template <typename U>
template <typename T, typename... Argument>
string FeatureConvenienceFunctions<U>::GET_CUMULATED_FLAG_evaluate(T condition, const string &flags,
                                                                    Argument... arguments) const
{
    string str{};
    if (static_cast<const U &>(*this).evaluate(condition))
    {
        str = flags;
    }
    if constexpr (sizeof...(arguments))
    {
        str += GET_FLAG_evaluate(arguments...);
    }
    return str;
}

template <typename U>
template <DepType dependency, typename T, typename... Condition>
void FeatureConvenienceFunctions<U>::RIGHT_MOST(T condition, Condition... conditions)
{
    if constexpr (sizeof...(conditions))
    {
        RIGHT_MOST(conditions...);
    }
    else
    {
        static_cast<U &>(*this).assign(condition);
    }
}

template <typename U>
template <DepType dependency, typename T, typename... Condition>
U &FeatureConvenienceFunctions<U>::assign(bool assign, T property, Condition... conditions)
{
    return assign ? static_cast<U &>(*this).assign(property, conditions...) : static_cast<U &>(*this);
}

template <typename U>
template <typename T, typename... Condition>
bool FeatureConvenienceFunctions<U>::AND(T condition, Condition... conditions) const
{
    if (!static_cast<const U &>(*this).evaluate(condition))
    {
        return false;
    }
    if constexpr (sizeof...(conditions))
    {
        return AND(conditions...);
    }
    else
    {
        return true;
    }
}

template <typename U>
template <typename T, typename... Condition>
bool FeatureConvenienceFunctions<U>::OR(T condition, Condition... conditions) const
{
    if (static_cast<const U &>(*this).evaluate(condition))
    {
        return true;
    }
    if constexpr (sizeof...(conditions))
    {
        return OR(conditions...);
    }
    else
    {
        return false;
    }
}

template <typename U>
template <DepType dependency, typename T, typename... Condition>
U &FeatureConvenienceFunctions<U>::M_LEFT_AND(T condition, Condition... conditions)
{
    if constexpr (sizeof...(conditions))
    {
        return static_cast<U &>(*this).evaluate(condition) ? M_LEFT_AND(conditions...) : *this;
    }
    else
    {
        static_cast<U &>(*this).assign(condition);
        return static_cast<U &>(*this);
    }
}

template <typename U>
template <DepType dependency, typename T, typename P>
U &FeatureConvenienceFunctions<U>::SINGLE(T condition, P property)
{
    if (static_cast<U &>(*this).evaluate(condition))
    {
        static_cast<U &>(*this).template assign<dependency>(property);
    }
    return static_cast<U &>(*this);
}

template <typename U>
template <DepType dependency, typename T, typename... Condition>
U &FeatureConvenienceFunctions<U>::M_LEFT_OR(T condition, Condition... conditions)
{
    if constexpr (sizeof...(conditions))
    {
        if (static_cast<U &>(*this).evaluate(condition))
        {
            RIGHT_MOST(conditions...);
            return static_cast<U &>(*this);
        }
        return M_LEFT_OR(conditions...);
    }
    else
    {
        return static_cast<U &>(*this);
    }
}

template <typename U>
template <DepType dependency, typename T, typename... Condition>
U &FeatureConvenienceFunctions<U>::M_RIGHT(T condition, Condition... conditions)
{
    if (static_cast<U &>(*this).evaluate(condition))
    {
        static_cast<U &>(*this).assign(conditions...);
    }
    return static_cast<U &>(*this);
}

#endif // HMAKE_FEATURESCONVENIENCEFUNCTIONS_HPP
