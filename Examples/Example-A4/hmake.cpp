#include "Configure.hpp"

struct OurTarget : BTarget
{
    string message;
    explicit OurTarget(string str) : message{std::move(str)}
    {
    }

    void completeRoundOne() override
    {
    }

    string getPrintName() const override
    {
        return message;
    }
};

void buildSpecification()
{
    OurTarget *a = new OurTarget("Cat1");
    OurTarget *b = new OurTarget("Cat2");
    OurTarget *c = new OurTarget("Cat3");
    a->addDep<0>(*b);
    b->addDep<0>(*c);
    c->addDep<0>(*a);
}

MAIN_FUNCTION