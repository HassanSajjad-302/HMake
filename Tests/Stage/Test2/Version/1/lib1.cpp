#include "private-lib1.hpp"
unsigned short getValueLib2();

unsigned short getValueLib1()
{
    return privateValueLib1 + getValueLib2();
}