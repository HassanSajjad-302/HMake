#include "private-lib3.hpp"
import "public-lib3.hpp";

unsigned short getValueLib3()
{
    return privateValueLib3 + getValueLib4();
}