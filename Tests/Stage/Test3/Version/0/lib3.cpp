#include "private-lib3.hpp"
#include "public-lib3.hpp"

unsigned short getValueLib3()
{
    return privateValueLib3 + getValueLib4();
}