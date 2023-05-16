#include "lib2/public-lib2.hpp"
#include "lib3/public-lib3.hpp"
#include "lib4/public-lib4.hpp"
#include "private-lib2.hpp"

unsigned short getValueLib2()
{
    return privateValueLib2 + getValueLib3() + getValueLib4();
}