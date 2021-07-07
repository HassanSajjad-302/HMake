#include "Goat.hpp"
#include "Dog.hpp"
#include "iostream"

void Goat::print() {
  Dog::print();
  std::cout << "Goat says baa" << std::endl;
}
