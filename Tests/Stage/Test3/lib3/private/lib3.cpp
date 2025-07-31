#include "private-lib3.hpp"
#include "public-lib3.hpp"

unsigned short getValueLib3()
{
    return PRIVATE_VALUE_LIB3 + getValueLib4();
}