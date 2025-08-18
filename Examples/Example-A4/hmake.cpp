#include "Configure.hpp"

struct OurTarget : public BTarget
{
    string message;
    explicit OurTarget(string str) : message{std::move(str)}
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round, bool &isComplete) override
    {
    }
};

OurTarget a("Hello");
OurTarget b("World");
OurTarget c("HMake");
void buildSpecification()
{
    a.addDependency<0>(b);
    b.addDependency<0>(c);
    c.addDependency<0>(a);
}

MAIN_FUNCTION