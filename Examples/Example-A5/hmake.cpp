#include "Configure.hpp"

constexpr unsigned short roundLocal = 0;
struct OurTarget : BTarget
{
    string name;
    bool error = false;
    explicit OurTarget(string name_, const bool error_ = false) : name{std::move(name_)}, error(error_)
    {
    }
    void updateBTarget(Builder &builder, const unsigned short round, bool &isComplete) override
    {
        if (round == roundLocal)
        {
            if (error)
            {
                printMessage(FORMAT("Target {} runtime error.\n", name));
                realBTargets[roundLocal].exitStatus = EXIT_FAILURE;
            }
            if (realBTargets[roundLocal].exitStatus == EXIT_SUCCESS)
            {
                printMessage(FORMAT("{}\n", name));
            }
        }
    }
};

void buildSpecification()
{
    OurTarget *a = new OurTarget("Hello");
    OurTarget *b = new OurTarget("World");
    OurTarget *c = new OurTarget("HMake");
    OurTarget *d = new OurTarget("CMake");
    OurTarget *e = new OurTarget("Ninja", true);
    OurTarget *f = new OurTarget("XMake");
    OurTarget *g = new OurTarget("build2", true);
    OurTarget *h = new OurTarget("Boost");
    d->addDepNow<roundLocal>(*e);
    h->addDepNow<roundLocal>(*g);
}

MAIN_FUNCTION