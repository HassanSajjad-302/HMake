#include "Configure.hpp"

int main() {
  Cache::initializeCache();
  Project project;
  ProjectVariant variantRelease;

  Executable fun("Fun", variantRelease);
  fun.sourceFiles.emplace_back("main.cpp");

  //PreBuild commands are executed before the build starts while the PostBuild  commands are executed after
  //the build of the target is finished. Cache::sourceDirectory.path and Cache::configureDirectory.path can be
  // provided here. Open both srcDir and buildDir in one frame and then run hbuild in terminal to see it.
  fun.preBuild.emplace_back("touch " + Cache::sourceDirectory.path.string() + "/file.txt");
  fun.preBuild.emplace_back("sleep 2");
  fun.postBuild.emplace_back("mv " + Cache::sourceDirectory.path.string()
                             + "/file.txt " + Cache::configureDirectory.path.string() + "file.txt");
  fun.postBuild.emplace_back("sleep 3");
  fun.postBuild.emplace_back("rm " + Cache::configureDirectory.path.string() + "/file.txt");

  variantRelease.executables.push_back(fun);
  project.projectVariants.push_back(variantRelease);
  project.configure();
  Cache::registerCacheVariables();
}