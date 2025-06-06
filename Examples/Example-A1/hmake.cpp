#include "Configure.hpp"

struct OurTarget : public BTarget
{
    string message;
    explicit OurTarget(string str) : message{std::move(str)}
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round) override
    {
        if(round == 0)
        {
            printMessage(FORMAT("{}\n", message));
        }
    }
};

OurTarget a("Hello");
OurTarget b("World");
void buildSpecification()
{
    b.addDependency<0>(a);
}

MAIN_FUNCTION

