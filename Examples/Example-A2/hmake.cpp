#include "Configure.hpp"

struct OurTarget : BTarget
{
    string message;
    explicit OurTarget(const string &str) : BTarget(str, false, BTargetType::UNKNOWN), message{str}
    {
    }

    void completeRoundOne() override
    {
        printMessage(FORMAT("{}\n", message));
    }

    bool isEventRegistered(Builder &buildeer) override
    {
        printMessage(FORMAT("{}\n", message));
        return false;
    }
};

void buildSpecification()
{
    OurTarget *a = new OurTarget("Hello");
    OurTarget *b = new OurTarget("World");

    b->addDep<0>(a);
    a->addDep<1>(b);
}

MAIN_FUNCTION