
#include "Configure.hpp"
#include "Executable.hpp"
#include "Initialize.hpp"
#include "Project.hpp"
int main(int argc, const char **argv) {
  initialize(argc, argv);
  Project revolution("Revolution");
  Executable evolution{"Evolution", File(Project::SOURCE_DIRECTORY.path / "main.cpp")};
  evolution.sourceFiles.emplace_back(File(Project::SOURCE_DIRECTORY.path / "func.cpp"));
  Project::projectExecutables.push_back(evolution);
  configure(revolution);
}
