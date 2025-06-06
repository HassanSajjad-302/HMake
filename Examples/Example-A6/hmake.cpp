#include "Configure.hpp"

struct OurTarget : public BTarget
{
    unsigned short low, high;
    explicit OurTarget(unsigned short low_, unsigned short high_) : low(low_), high(high_)
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round) override
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
    void updateBTarget(Builder &builder, unsigned short round) override
    {
        if (round == 0)
        {
            a = new OurTarget(10, 40);
            b = new OurTarget(50, 80);
            c = new OurTarget(800, 1000);
            a->addDependency<0>(*c);
            b->addDependency<0>(*c);

            {
                std::lock_guard<std::mutex> lk(builder.executeMutex);
                builder.updateBTargetsIterator = builder.updateBTargets.emplace(builder.updateBTargetsIterator, c);
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