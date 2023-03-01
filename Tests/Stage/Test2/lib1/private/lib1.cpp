#include "private-lib1.hpp"
#include "public-lib1.hpp"
#include "public-lib2.hpp"

unsigned short getValueLib1()
{
    return privateValueLib1 + getValueLib2();
}