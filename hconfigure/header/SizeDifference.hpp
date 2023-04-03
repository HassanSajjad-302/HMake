
#ifndef HMAKE_SIZEDIFFERENCE_HPP
#define HMAKE_SIZEDIFFERENCE_HPP

#ifdef USE_HEADER_UNITS
import "BasicTargets.hpp";
#else
#include "BasicTargets.hpp"
#endif

using std::function;
template <typename PreSortType = void(Builder &, unsigned short round),
          typename DuringSortType = void(Builder &, unsigned short round, unsigned int),
          typename UpdateBTargetType = void(Builder &, unsigned short round)>
struct RoundZeroBTarget : public BTarget
{
    function<PreSortType> preSortFunctor = nullptr;
    function<DuringSortType> duringSortFunctor = nullptr;
    function<UpdateBTargetType> updateFunctor = nullptr;

    RoundZeroBTarget()
    {
        getRealBTarget(0).fileStatus = FileStatus::NEEDS_UPDATE;
    }
    void setPreSortFunctor(function<PreSortType> f)
    {
        preSortFunctor = std::move(f);
    }
    void setDuringSortFunctor(function<PreSortType> f)
    {
        duringSortFunctor = std::move(f);
    }
    void setUpdateFunctor(function<PreSortType> f)
    {
        updateFunctor = std::move(f);
    }

    void preSort(class Builder &builder, unsigned short round) override
    {
        if (updateFunctor)
        {
            updateFunctor(builder, round);
        }
    }
    void duringSort(class Builder &builder, unsigned short round,
                    unsigned int indexInTopologicalSortComparator) override
    {
        if (duringSortFunctor)
        {
            duringSortFunctor(builder, round, indexInTopologicalSortComparator);
        }
    }
    void updateBTarget(class Builder &builder, unsigned short round) override
    {
        if (updateFunctor)
        {
            updateFunctor(builder, round);
        }
        getRealBTarget(0).fileStatus = FileStatus::UPDATED;
    }
};

template <typename T = void(Builder &, unsigned short round),
          typename U = void(Builder &, unsigned short round, unsigned int),
          typename V = void(Builder &, unsigned short round)>
struct CTargetRoundZeroBTarget : public RoundZeroBTarget<T, U, V>, public CTarget
{
    explicit CTargetRoundZeroBTarget(const string &name_) : CTarget(name_)
    {
    }
    CTargetRoundZeroBTarget(const string &name_, CTarget &container, bool hasFile = true)
        : CTarget(name_, container, hasFile)
    {
    }
};

#endif // HMAKE_SIZEDIFFERENCE_HPP
