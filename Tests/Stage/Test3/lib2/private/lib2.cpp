#include "private-lib2.hpp"
#include "public-lib2.hpp"
import "public-lib3.hpp";
import "public-lib4.hpp";

unsigned short getValueLib2()
{
    return privateValueLib2 + getValueLib3() + getValueLib4();
}