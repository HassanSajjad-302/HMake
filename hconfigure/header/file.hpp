#ifndef FILE_HPP
#define FILE_HPP

#include<filesystem>
#include<iostream>
#include<vector>
#include"directory.hpp"
namespace fs = std::filesystem;
struct file
{
    fs::path path;
    file(fs::path path);
    std::string getScript();
};

class FileArray{
    std::string fileNames;
public:
    FileArray(directory dir, bool(*predicate)(std::string) = [](std::string){return true;});
    std::string getScript();
};
#endif // FILE_HPP
