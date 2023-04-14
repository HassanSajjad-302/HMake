#include "Dog.hpp"
#include "Cat.hpp"
#include <iostream>

DOG2_EXPORT void Dog::print()
{
    Cat::print();
    std::cout << "Dog says Woof.." << std::endl;
}
