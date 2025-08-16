#include "private-lib2.hpp"
#include "public-lib2.hpp"
import "public-lib3.hpp";
import "public-lib4.hpp";

unsigned short getValueLib2()
{
    return PRIVATE_VALUE_LIB2 + getValueLib3() + getValueLib4();
}