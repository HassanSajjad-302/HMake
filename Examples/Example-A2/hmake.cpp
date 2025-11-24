#include "Configure.hpp"

struct OurTarget : BTarget
{
    string message;
    explicit OurTarget(string str) : message{std::move(str)}
    {
    }
    void updateBTarget(Builder &builder, const unsigned short round, bool &isComplete) override
    {
        if (round == 0 || round == 1)
        {
            printMessage(FORMAT("{}\n", message));
        }
    }
};

void buildSpecification()
{
    OurTarget *a = new OurTarget("Hello");
    OurTarget *b = new OurTarget("World");

    b->addDepNow<0>(*a);
    a->addDepNow<1>(*b);
}

MAIN_FUNCTION