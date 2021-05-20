

#include "build.hpp"
#include "fstream"
#include "iostream"
void build(fs::path path) {
    std::ifstream p(path);
    std::cout<<p.rdbuf()<<std::endl;
}
