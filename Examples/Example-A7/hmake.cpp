#include "Configure.hpp"

BTarget *b, *c;
struct OurTarget : BTarget
{
    bool isEventRegistered(Builder &builder) override
    {
        b->addDep<0>(*c);
        c->addDep<0>(*b);
        return false;
    }
};

void buildSpecification()
{
    b = new BTarget();
    c = new BTarget();
    OurTarget *target = new OurTarget();
    b->addDep<0>(*target);
    c->addDep<0>(*target);
}

MAIN_FUNCTION