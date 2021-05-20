
#include "executable.hpp"
#include "configure.hpp"
#include "project.hpp"
#include "initialize.hpp"
int main(int argc, const char**argv){
    initialize(argc, argv);
    project revolution("Revolution");
    executable evolution{"Evolution", file(project::SOURCE_DIRECTORY.path / "main.cpp")};
    evolution.sourceFiles.emplace_back(file(project::SOURCE_DIRECTORY.path/"func.cpp"));
    project::projectExecutables.push_back(evolution);
    configure(revolution);
}
