#ifdef USE_HEADER_UNIT
import "func.hpp";
#else
#include "func.hpp"
#endif
string func1()
{
    return "func1";
}