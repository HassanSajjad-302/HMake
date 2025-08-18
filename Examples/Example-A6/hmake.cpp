#include "Configure.hpp"

struct OurTarget : public BTarget
{
    unsigned short low, high;
    explicit OurTarget(unsigned short low_, unsigned short high_) : low(low_), high(high_), BTarget(true, false, false)
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round, bool &isComplete) override
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

struct OurTarget2 : public BTarget
{
    void updateBTarget(Builder &builder, unsigned short round, bool &isComplete) override
    {
        if (round == 0)
        {
            a = new OurTarget(10, 40);
            b = new OurTarget(50, 80);
            c = new OurTarget(800, 1000);
            a->addDepNow<0>(*c);
            b->addDepNow<0>(*c);

            {
                std::lock_guard lk(builder.executeMutex);
                builder.updateBTargets.emplace(c);
                builder.updateBTargetsSizeGoal += 3;
            }
            builder.cond.notify_one();
        }
    }
};

OurTarget2 target2;

void buildSpecification()
{
}

MAIN_FUNCTION