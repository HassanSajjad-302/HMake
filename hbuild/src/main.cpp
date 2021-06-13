
#include "BBuild.hpp"

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
    fs::path projectFile;
    fs::path targetFile;
    int count = 0;
    for (auto &i : fs::directory_iterator(fs::current_path())) {
      if (i.is_regular_file()) {
        std::string fileName = i.path().filename();
        if (fileName == "Project.hmake") {
          projectFile = i;
          count = 0;
          break;
        }
        if (fileName.ends_with(".executable.hmake") || fileName.ends_with(".static.hmake")
            || fileName.ends_with(".shared.hmake")) {
          targetFile = i;
          ++count;
        }
      }
    }
    if (count == 2) {
      throw std::runtime_error("Could not find or determine the target or project file which is to be built.");
    }
    if (count == 0) {
      BProject project(projectFile);
      project.build();
    } else if (count == 1) {
      BProject project(BProject::getProjectFilePathFromTargetFilePath(targetFile));
      BTargetBuilder builder(project);
      builder.initializeTargetTypeAndParseJsonAndBuild(targetFile);
    }
  }
}
