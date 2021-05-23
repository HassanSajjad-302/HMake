#ifndef FILE_HPP
#define FILE_HPP

#include "Directory.hpp"
#include <filesystem>
#include <iostream>
#include <vector>
namespace fs = std::filesystem;
struct File {
  fs::path path;
  File(const fs::path& path);
};

class FileArray {
  std::string fileNames;

public:
  FileArray(
      Directory dir, bool (*predicate)(std::string) = [](std::string) { return true; });
};
#endif// FILE_HPP
