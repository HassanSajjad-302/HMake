
#include "File.hpp"
#include "Project.hpp"
#include "filesystem"
File::File(std::filesystem::__cxx11::path path_) {
  if (path_.is_relative()) {
    path_ = Project::SOURCE_DIRECTORY.path / path_;
  }
  if (fs::directory_entry(path_).status().type() == fs::file_type::regular) {
    path = path_;
    return;
  }
  throw std::runtime_error(path_.string() + " Is Not a Regular File");
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