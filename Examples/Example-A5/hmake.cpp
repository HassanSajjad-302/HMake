#include "Configure.hpp"

constexpr unsigned short roundLocal = 1;
struct OurTarget : public BTarget
{
    string message;
    bool error = false;
    explicit OurTarget(string str, bool error_ = false) : message{std::move(str)}, error(error_)
    {
    }
    void updateBTarget(class Builder &builder, unsigned short round) override
    {
        if (round == roundLocal)
        {
            if (error)
            {
                throw std::runtime_error("Target " + message + " runtime error.\n");
            }
            if (realBTargets[roundLocal].exitStatus == EXIT_SUCCESS)
            {
                printMessage(FORMAT("{}\n", message));
            }
        }
    }
};

OurTarget a("Hello"), b("World"), c("HMake"), d("CMake"), e("Ninja", true), f("XMake"), g("build2", true), h("Boost");
void buildSpecification()
{
    d.addDependency<roundLocal>(e);
    h.addDependency<roundLocal>(g);
}

MAIN_FUNCTION