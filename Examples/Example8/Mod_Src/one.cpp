#include "iostream"
import two;
import three;

void func1()
{
    func2();
    func3();
}

int main()
{
    func1();
    std::cout << "Hello World" << std::endl;
}
