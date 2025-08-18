#include "Configure.hpp"

BTarget b, c;

struct OurTarget : public BTarget
{
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override
    {
        if (round == 0)
        {
            b.addDependency<0>(c);
            c.addDependency<0>(b);
        }
    }
};

OurTarget target;

void buildSpecification()
{
    b.addDependency<0>(target);
    c.addDependency<0>(target);
}

MAIN_FUNCTION