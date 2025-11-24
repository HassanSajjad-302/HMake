#include "Configure.hpp"

struct OurTarget : BTarget
{
    unsigned short low, high;
    explicit OurTarget(const unsigned short low_, const unsigned short high_)
        : BTarget(true, false), low(low_), high(high_)
    {
    }
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override
    {
        if (round == 0)
        {
            for (unsigned short i = low; i < high; ++i)
            {
                printMessage(FORMAT("{} ", i));
                // To simulate a long-running task that continuously outputs.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
};

OurTarget *a, *b, *c;

struct OurTarget2 : BTarget
{
    void updateBTarget(Builder &builder, const unsigned short round, bool &isComplete) override
    {
        if (round == 0)
        {
            a = new OurTarget(10, 40);
            b = new OurTarget(50, 80);
            c = new OurTarget(800, 1000);
            a->addDepNow<0>(*c);
            b->addDepNow<0>(*c);

            std::lock_guard lk(builder.executeMutex);
            builder.updateBTargets.emplace(&c->realBTargets[0]);
            builder.updateBTargetsSizeGoal += 3;
        }
    }
};

void buildSpecification()
{
    OurTarget2 *target2 = new OurTarget2();
}

MAIN_FUNCTION