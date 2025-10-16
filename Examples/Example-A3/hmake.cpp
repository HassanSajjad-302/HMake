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

OurTarget a(10, 20), b(50, 70), c(800, 1000);
void buildSpecification()
{
}

MAIN_FUNCTION
