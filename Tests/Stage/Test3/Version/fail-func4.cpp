#ifdef USE_HEADER_UNIT
import "func.hpp";
#else
#include "func.hpp"
#endif
string func4()
{
    return "func4";
}

#ifdef FAIL_THE_BUILD
sdf
#endif