#include "Configure.hpp"

BTarget b, c;

struct OurTarget : public BTarget
{
    void updateBTarget(Builder &builder, unsigned short round) override
    {
        if (round == 0)
        {
            b.realBTargets[0].addDependency(c);
            c.realBTargets[0].addDependency(b);
        }
    }
};

OurTarget target;

void buildSpecification()
{
    b.realBTargets[0].addDependency(target);
    c.realBTargets[0].addDependency(target);
}

MAIN_FUNCTION