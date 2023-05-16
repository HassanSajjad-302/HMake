import "lib2/public-lib2.hpp";
import "lib3/public-lib3.hpp";
import "lib4/public-lib4.hpp";
import "private-lib2.hpp";

unsigned short getValueLib2()
{
    return privateValueLib2 + getValueLib3() + getValueLib4();
}