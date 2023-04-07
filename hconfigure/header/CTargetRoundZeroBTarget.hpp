
#ifndef HMAKE_CTARGETROUNDZEROBTARGET_HPP
#define HMAKE_CTARGETROUNDZEROBTARGET_HPP

#ifdef USE_HEADER_UNITS
import "BasicTargets.hpp";
#else
#include "BasicTargets.hpp"
#endif

using std::function;
template <typename UpdateBTargetType> struct RoundZeroBTarget : public BTarget
{
    function<UpdateBTargetType> updateFunctor = nullptr;
    RealBTarget &realBTarget;

    RoundZeroBTarget() : realBTarget(getRealBTarget(0))
    {
        realBTarget.fileStatus = FileStatus::NEEDS_UPDATE;
    }

    void setUpdateFunctor(function<UpdateBTargetType> f)
    {
        updateFunctor = std::move(f);
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

template <typename T> struct CTargetRoundZeroBTarget : public RoundZeroBTarget<T>, public CTarget
{
    explicit CTargetRoundZeroBTarget(const string &name_) : CTarget(name_)
    {
    }
    CTargetRoundZeroBTarget(const string &name_, CTarget &container, bool hasFile = true)
        : CTarget(name_, container, hasFile)
    {
    }
};

#endif // HMAKE_CTARGETROUNDZEROBTARGET_HPP
