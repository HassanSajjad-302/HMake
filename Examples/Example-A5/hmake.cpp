#include "Configure.hpp"

struct OurTarget : BTarget
{
    string name;
    bool error = false;
    explicit OurTarget(string name_, const bool error_ = false) : name{std::move(name_)}, error(error_)
    {
    }

    bool isEventRegistered(Builder &builder) override
    {
        if (error)
        {
            printMessage(FORMAT("Target {} runtime error.\n", name));
            realBTargets[0].exitStatus = EXIT_FAILURE;
        }

        if (realBTargets[0].exitStatus == EXIT_SUCCESS)
        {
            printMessage(FORMAT("{}\n", name));
        }
        return false;
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
    d->addDep<0>(*e);
    h->addDep<0>(*g);
}

MAIN_FUNCTION