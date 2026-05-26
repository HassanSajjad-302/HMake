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
};

void buildSpecification()
{
    OurTarget *a = new OurTarget("Hello");
    OurTarget *b = new OurTarget("World");
    b->addDep<1>(a);
}

MAIN_FUNCTION
