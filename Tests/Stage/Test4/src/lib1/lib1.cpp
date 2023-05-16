#include "lib1/public-lib1.hpp"
#include "private-lib1.hpp"

unsigned short getValueLib1()
{
    return privateValueLib1 + getValueLib2();
}