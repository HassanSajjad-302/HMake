#include "private-lib1.hpp"
#include "public-lib1.hpp"

unsigned short getValueLib1()
{
    return PRIVATE_VALUE_LIB1 + getValueLib2();
}