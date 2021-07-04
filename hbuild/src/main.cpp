
#include "BBuild.hpp"
#include "fstream"

int main(int argc, char **argv) {
  if (argc == 2) {
    std::string filePath = argv[1];
    fs::path buildFilePath = filePath;
    if (buildFilePath.is_relative()) {
      buildFilePath = (fs::current_path() / buildFilePath).lexically_normal();
    } else {
      buildFilePath = buildFilePath.lexically_normal();
    }
    if (!fs::is_regular_file(buildFilePath)) {
      throw std::logic_error(buildFilePath.string() + " is not regular file");
    }

  } else {
    for (auto &file : fs::directory_iterator(fs::current_path())) {
      if (file.is_regular_file()) {
        std::string fileName = file.path().filename();
        if (fileName == "projectVariant.hmake" || fileName == "packageVariant.hmake") {
          BVariant{file.path()};
        } else if (fileName == "project.hmake") {
          BProject{file.path()};
        } else if (fileName == "cache.hmake") {
          BProject{fs::canonical(file.path()).parent_path() / "project.hmake"};
        } else if (fileName == "package.hmake") {
          BPackage{file.path()};
        } else if (fileName == "Common.hmake") {
          BPackage{fs::canonical(file.path()).parent_path() / "package.hmake"};
        } else if (fileName.ends_with(".hmake")) {
          BTarget{file.path()};
        } else {
          throw std::runtime_error("Could not find or determine the target or projectVariant or packageVariant "
                                   "or package file which is to be built.");
        }
        break;
      }
    }
  }
}
