#include "Configure.hpp"

struct OurTarget : public BTarget
{
    string message;
    explicit OurTarget(string str) : message{std::move(str)}
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round) override
    {
    }
};

OurTarget a("Hello");
OurTarget b("World");
OurTarget c("HMake");
void buildSpecification()
{
    a.realBTargets[0].addDependency(b);
    b.realBTargets[0].addDependency(c);
    c.realBTargets[0].addDependency(a);
}

MAIN_FUNCTION
