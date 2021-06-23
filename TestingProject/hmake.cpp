
#include "Configure.hpp"

int main() {
  ::flags = Flags::defaultFlags();
  Cache::initializeCache();
  ProjectVariant project{};
  Executable sample("Sample", project);
  sample.sourceFiles.emplace_back("main.cpp");
  if (CacheVariable("FILE1", true).value) {
    sample.sourceFiles.emplace_back("file1.cpp");
  } else {
    sample.sourceFiles.emplace_back("file2.cpp");
  }
  project.executables.push_back(sample);
  project.configure();
  Cache::registerCacheVariables();
}