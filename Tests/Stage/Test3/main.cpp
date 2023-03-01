#ifdef USE_HEADER_UNIT
import "func.hpp";
import <iostream>;
#else
#include "func.hpp"
#include <iostream>
#endif

int main()
{
    std::cout << func1() /*<< func2() << func3() << func4()*/ << std::endl;
}