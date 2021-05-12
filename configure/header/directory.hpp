#ifndef DIRECTORY_HPP
#define DIRECTORY_HPP

#include<filesystem>
#include<iostream>
enum commonDirectories{
    SOURCE_DIR, BUILD_DIR, CURRENT_SOURCE_DIR, CURRENT_BINARY_DIR
};
namespace fs = std::filesystem;
struct directory
{
    fs::path path;
    directory(fs::path path);
    directory(fs::path path, fs::path relative_to);
    directory(fs::path path, commonDirectories relative_to);
    std::string getScript();
};

#endif // DIRECTORY_HPP
