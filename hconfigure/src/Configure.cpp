
#include "Configure.hpp"
#include "fstream"

void configure(const Library &library) {
  Json json;
  json = library;
  fs::path p = library.configureDirectory.path;
  std::string fileName;
  if (library.libraryType == LibraryType::STATIC) {
    fileName = library.targetName + ".static.hmake";
  } else {
    fileName = library.targetName + ".shared.hmake";
  }
  p /= fileName;
  std::ofstream(p) << json.dump(4);
  for (auto &t : Project::projectExecutables) {
    configure(t);
  }
  for (auto &t : Project::projectLibraries) {
    configure(t);
  }
}

void configure(const Executable &executable) {
  Json json;
  json = executable;
  fs::path p = executable.configureDirectory.path;
  std::string fileName = executable.targetName + ".executable.hmake";
  p /= fileName;
  std::ofstream(p) << json.dump(4);
  for (auto &t : Project::projectExecutables) {
    configure(t);
  }
  for (auto &t : Project::projectLibraries) {
    configure(t);
  }
}

void configure(Project project) {
  Json json;
  json = project;
  fs::path p = Project::BUILD_DIRECTORY.path;
  std::string fileName = Project::PROJECT_NAME + ".hmake";
  p /= fileName;
  std::ofstream(p) << json.dump(4);
  for (auto &t : Project::projectExecutables) {
    configure(t);
  }
  for (auto &t : Project::projectLibraries) {
    configure(t);
  }
}
