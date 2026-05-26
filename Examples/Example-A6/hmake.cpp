#include "Configure.hpp"

struct OurTarget : BTarget
{
    unsigned short low, high;
    explicit OurTarget(const string &str, const unsigned short low_, const unsigned short high_)
        : BTarget(str, false, BTargetType::UNKNOWN, false, false, false, false), low(low_), high(high_)
    {
    }

    bool isEventRegistered(Builder &builder) override
    {
        for (unsigned short i = low; i < high; ++i)
        {
            printMessage(FORMAT("{} ", i));
        }
        return false;
    }
};

OurTarget *a, *b, *c;

struct OurTarget2 : BTarget
{
    explicit OurTarget2(const string &str) : BTarget(str, false, BTargetType::UNKNOWN)
    {
        a = new OurTarget("a", 10, 40);
        b = new OurTarget("b", 50, 80);
        c = new OurTarget("c", 800, 1000);
    }

    bool isEventRegistered(Builder &builder) override
    {
        a->addDep<0>(c);
        b->addDep<0>(c);

        uint32_t insertionIndex;
        builder.updateBTargets.emplace(&c->realBTargets[0], insertionIndex);
        builder.updateBTargetsSizeGoal += 3;
        return false;
    }
};

void buildSpecification()
{
    OurTarget2 *target2 = new OurTarget2("target2");
}

MAIN_FUNCTION