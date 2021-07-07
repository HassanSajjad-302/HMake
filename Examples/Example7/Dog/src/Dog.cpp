
#include "Dog.hpp"
#include "Cat.hpp"
#include "iostream"
void Dog::print() {
  std::cout << "Dog says woof" << std::endl;
  Cat::print();
}