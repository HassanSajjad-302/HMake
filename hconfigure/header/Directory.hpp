#ifndef DIRECTORY_HPP
#define DIRECTORY_HPP

#include <filesystem>
#include <iostream>
enum CommonDirectories {
  SOURCE_DIR,
  BUILD_DIR,
  CURRENT_SOURCE_DIR,
  CURRENT_BINARY_DIR
};
namespace fs = std::filesystem;
struct Directory {
  fs::path path;
  Directory();
  explicit Directory(fs::path path);
};

#endif// DIRECTORY_HPP
