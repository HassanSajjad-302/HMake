#include "Configure.hpp"

struct OurTarget : public BTarget
{
    string message;
    explicit OurTarget(string str) : message{std::move(str)}
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round) override
    {
        if(round == 0 || round == 1)
        {
            printMessage(FORMAT("{}\n", message));
        }
    }
};

OurTarget a("Hello");
OurTarget b("World");
void buildSpecification()
{
    b.realBTargets[0].addDependency(a);
    a.realBTargets[1].addDependency(b);
}

MAIN_FUNCTION