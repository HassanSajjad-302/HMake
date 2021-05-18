#ifndef TARGET_HPP
#define TARGET_HPP


#include<vector>

#include"directory.hpp"

class Target{
    std::string include_directories;
    std::vector<Directory> direc_vec;
public:
    void add_include_directory(IncludeDirectory dir){

    }
};





#endif // TARGET_HPP
