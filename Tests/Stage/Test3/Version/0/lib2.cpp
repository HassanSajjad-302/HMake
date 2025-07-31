#include "private-lib2.hpp"
#include "public-lib2.hpp"
#include "public-lib3.hpp"
#include "public-lib4.hpp"

unsigned short getValueLib2()
{
    return PRIVATE_VALUE_LIB2 + getValueLib3() + getValueLib4();
}