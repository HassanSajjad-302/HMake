#include "public-lib2.hpp"

unsigned short getValueLib1();
int main()
{
    std::cout << getValueLib1() + getValueLib2() << std::endl;
}