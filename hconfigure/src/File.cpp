
#include "File.hpp"
#include <filesystem>
File::File(const std::filesystem::__cxx11::path &path) {
  if (fs::directory_entry(path).status().type() == fs::file_type::regular) {
    this->path = path;
    return;
  }
  throw std::runtime_error(path.string() + " Is Not a Regular File");
}

FileArray::FileArray(Directory dir, bool (*predicate)(std::string)) {
  for (auto &&x : fs::directory_iterator(dir.path)) {
    if (x.status().type() == fs::file_type::regular) {
      if (predicate(x.path().filename().generic_string())) {
        fileNames += x.path().filename().generic_string() + "\n";
      }
    }
  }
}