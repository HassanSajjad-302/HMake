#ifdef USE_HEADER_UNITS
import "BasicTargets.hpp";
import "IDE_API.hpp";
#else
#include "BasicTargets.hpp"
#endif

extern "C" EXPORT C_TargetContainer *getCTargetContainer(BSMode bsModeLocal)
{
    auto *c_cTargetContainer = new C_TargetContainer();
    c_cTargetContainer->size = targetPointers<CTarget>.size();
    c_cTargetContainer->c_cTargets = new C_Target *[c_cTargetContainer->size];

    unsigned short i = 0;
    for (CTarget *target : targetPointers<CTarget>)
    {
        c_cTargetContainer->c_cTargets[i] = target->get_CAPITarget(bsModeLocal);
        ++i;
    }

    return c_cTargetContainer;
}