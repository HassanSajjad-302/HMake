#include "Configure.hpp"

BTarget *b, *c;
struct OurTarget : BTarget
{
    void updateBTarget(Builder &builder, const unsigned short round, bool &isComplete) override
    {
        if (round == 0)
        {
            b->addDepNow<0>(*c);
            c->addDepNow<0>(*b);
        }
    }
};

void buildSpecification()
{
    b = new BTarget();
    c = new BTarget();
    OurTarget *target = new OurTarget();
    b->addDepNow<0>(*target);
    c->addDepNow<0>(*target);
}

MAIN_FUNCTION