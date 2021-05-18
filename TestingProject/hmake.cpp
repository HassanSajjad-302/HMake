
#include "executable.hpp"
#include "configure.hpp"
#include "project.hpp"
#include "initialize.hpp"
int main(int argc, const char**argv){
    initialize(argc, argv);
    executable executable{"basic", file("main.cpp")};
    project basic("basic");
    basic.projectExecutables.push_back(executable);
    configure(basic);
}
