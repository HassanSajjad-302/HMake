import "lib1/public-lib1.hpp";
import "private-lib1.hpp";

unsigned short getValueLib1()
{
    return privateValueLib1 + getValueLib2();
}