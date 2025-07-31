#include "private-lib3.hpp"
import "public-lib3.hpp";

unsigned short getValueLib3()
{
    return PRIVATE_VALUE_LIB3 + getValueLib4();
}