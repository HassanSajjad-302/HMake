#include "Configure.hpp"

BTarget *b, *c;
struct OurTarget : BTarget
{
    explicit OurTarget(const string &str) : BTarget(str, false, BTargetType::UNKNOWN){}
    bool isEventRegistered(Builder &builder) override
    {
        b->addDep<0>(c);
        c->addDep<0>(b);
        return false;
    }
};

void buildSpecification()
{
    b = new BTarget("b", false, BTargetType::UNKNOWN);
    c = new BTarget("c", false, BTargetType::UNKNOWN);
    OurTarget *target = new OurTarget("target");
    b->addDep<0>(target);
    c->addDep<0>(target);
}

MAIN_FUNCTION