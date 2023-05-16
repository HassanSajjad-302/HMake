#include "lib3/public-lib3.hpp"
#include "private-lib3.hpp"

unsigned short getValueLib3()
{
    return privateValueLib3 + getValueLib4();
}