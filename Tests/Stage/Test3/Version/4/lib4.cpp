#ifdef USE_MODULE
import "private-lib4.hpp";
import "public-lib4.hpp";
#else
#include "private-lib4.hpp"
#include "public-lib4.hpp"
#endif
unsigned short getValueLib4()
{
    return privateValueLib4;
}