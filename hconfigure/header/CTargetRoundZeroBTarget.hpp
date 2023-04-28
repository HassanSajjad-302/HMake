
#ifndef HMAKE_CTARGETROUNDZEROBTARGET_HPP
#define HMAKE_CTARGETROUNDZEROBTARGET_HPP

#ifdef USE_HEADER_UNITS
import "BasicTargets.hpp";
#else
#include "BasicTargets.hpp"
#endif

using std::function;
struct RoundZeroUpdateBTarget : public BTarget
{
    function<void(Builder &, unsigned short, BTarget &bTarget)> updateFunctor = nullptr;

    explicit RoundZeroUpdateBTarget(function<void(Builder &, unsigned short, BTarget &bTarget)> updateFunctor_)
        : updateFunctor(std::move(updateFunctor_))
    {
        getRealBTarget(0).fileStatus = FileStatus::NEEDS_UPDATE;
    }

    void updateBTarget(class Builder &builder, unsigned short round) override
    {
        if (!round)
        {
            updateFunctor(builder, round, *this);
            getRealBTarget(0).fileStatus = FileStatus::UPDATED;
        }
    }
};

struct CTargetRoundZeroBTarget : public RoundZeroUpdateBTarget, public CTarget
{
    explicit CTargetRoundZeroBTarget(const string &name_,
                                     function<void(Builder &, unsigned short, BTarget &bTarget)> updateFunctor_)
        : RoundZeroUpdateBTarget(std::move(updateFunctor_)), CTarget(name_)
    {
    }
    CTargetRoundZeroBTarget(const string &name_, CTarget &container, bool hasFile,
                            function<void(Builder &, unsigned short, BTarget &bTarget)> updateFunctor_)
        : RoundZeroUpdateBTarget(std::move(updateFunctor_)), CTarget(name_, container, hasFile)
    {
    }
};

#endif // HMAKE_CTARGETROUNDZEROBTARGET_HPP
