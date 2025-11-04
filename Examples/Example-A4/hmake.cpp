#include "Configure.hpp"

struct OurTarget : BTarget
{
    string message;
    explicit OurTarget(string str) : message{std::move(str)}
    {
    }
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override
    {
    }
};

void buildSpecification()
{
    OurTarget *a = new OurTarget("Hello");
    OurTarget *b = new OurTarget("World");
    OurTarget *c = new OurTarget("HMake");
    a->addDepNow<0>(*b);
    b->addDepNow<0>(*c);
    c->addDepNow<0>(*a);
}

MAIN_FUNCTION