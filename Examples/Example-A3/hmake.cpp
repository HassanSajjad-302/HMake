#include "Configure.hpp"

struct OurTarget : BTarget
{
    unsigned short low, high;
    explicit OurTarget(const unsigned short low_, const unsigned short high_) : low(low_), high(high_)
    {
    }
    void updateBTarget(Builder &builder, const unsigned short round, bool &isComplete) override
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

void buildSpecification()
{
    OurTarget *a = new OurTarget(10, 20);
    OurTarget *b = new OurTarget(50, 70);
    OurTarget *c = new OurTarget(800, 1000);
}

MAIN_FUNCTION
