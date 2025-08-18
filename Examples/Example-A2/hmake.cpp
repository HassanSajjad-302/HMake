#include "Configure.hpp"

struct OurTarget : public BTarget
{
    string message;
    explicit OurTarget(string str) : message{std::move(str)}
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round, bool &isComplete) override
    {
        if (round == 0 || round == 1)
        {
            printMessage(FORMAT("{}\n", message));
        }
    }
};

OurTarget a("Hello");
OurTarget b("World");
void buildSpecification()
{
    b.addDepNow<0>(a);
    a.addDepNow<1>(b);
}

MAIN_FUNCTION