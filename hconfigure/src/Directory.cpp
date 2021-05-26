#include "Directory.hpp"
#include "Project.hpp"

Directory::Directory() {
  path = fs::current_path();
  path /= "";
}

Directory::Directory(fs::path path_) {
  path = path_.lexically_normal();
  if (path_.is_relative()) {
    path_ = Project::SOURCE_DIRECTORY.path / path_;
  }
  if (fs::directory_entry(path_).status().type() == fs::file_type::directory) {
    path = path_;
  } else {
    throw std::logic_error(path_.string() + " Is Not a directory file");
  }
  path /= "";
}