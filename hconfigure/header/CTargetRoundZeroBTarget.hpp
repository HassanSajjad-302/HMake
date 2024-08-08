
#ifndef HMAKE_CTARGETROUNDZEROBTARGET_HPP
#define HMAKE_CTARGETROUNDZEROBTARGET_HPP

#ifdef USE_HEADER_UNITS
import "BTarget.hpp";
#else
#include "BTarget.hpp"
#endif

using std::function;
struct RoundZeroUpdateBTarget : BTarget
{
    function<void(Builder &, BTarget &bTarget)> updateFunctor = nullptr;

    explicit RoundZeroUpdateBTarget(function<void(Builder &, BTarget &bTarget)> updateFunctor_)
        : updateFunctor(std::move(updateFunctor_))
    {
    }

    void updateBTarget(Builder &builder, unsigned short round) override
    {
        if (!round)
        {
            updateFunctor(builder, *this);
        }
    }
};

#endif // HMAKE_CTARGETROUNDZEROBTARGET_HPP
