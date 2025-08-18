#include "Configure.hpp"

constexpr unsigned short roundLocal = 1;
struct OurTarget : public BTarget
{
    string name;
    bool error = false;
    explicit OurTarget(string name_, bool error_ = false) : name{std::move(name_)}, error(error_)
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round, bool &isComplete) override
    {
        if (round == roundLocal)
        {
            if (error)
            {
                fmt::print("Target {} runtime error.\n", name);
                realBTargets[roundLocal].exitStatus = EXIT_FAILURE;
            }
            if (realBTargets[roundLocal].exitStatus == EXIT_SUCCESS)
            {
                printMessage(FORMAT("{}\n", name));
            }
        }
    }
};

OurTarget a("Hello"), b("World"), c("HMake"), d("CMake"), e("Ninja", true), f("XMake"), g("build2", true), h("Boost");
void buildSpecification()
{
    d.addDepNow<roundLocal>(e);
    h.addDepNow<roundLocal>(g);
}

MAIN_FUNCTION