#include "Configure.hpp"

int main()
{
    Cache::initializeCache();
    Project project;
    ProjectVariant variantRelease;

    Executable fun("Fun", variantRelease);
    fun.sourceFiles.emplace_back("main.cpp");

    // PreBuild commands are executed before the build starts while the PostBuild  commands are executed after
    // the build of the target is finished. Cache::sourceDirectory.path and Cache::configureDirectory.path can be
    //  provided here. Open both srcDir and buildDir in one frame and then run hbuild in terminal to see it.
#ifdef _WIN32
    fun.preBuild.emplace_back("echo > " +
                              addQuotes((Cache::sourceDirectory.directoryPath / "file.txt").make_preferred().string()));
    fun.preBuild.emplace_back("ping 127.0.0.1 -n 2 > nul"); // sleep command windows alternative
    fun.postBuild.emplace_back(
        "move " + addQuotes((Cache::sourceDirectory.directoryPath / "file.txt").make_preferred().string()) + " " +
        addQuotes((Cache::configureDirectory.directoryPath / "file.txt").make_preferred().string()));
    fun.postBuild.emplace_back("ping 127.0.0.1 -n 3 > nul");
    fun.postBuild.emplace_back(
        "del " + addQuotes((Cache::configureDirectory.directoryPath / "file.txt").make_preferred().string()));
#else
    fun.preBuild.emplace_back("touch " +
                              addQuotes((Cache::sourceDirectory.directoryPath / "file.txt").make_preferred().string()));
    fun.preBuild.emplace_back("sleep 2");
    fun.postBuild.emplace_back(
        "mv " + addQuotes((Cache::sourceDirectory.directoryPath / "file.txt").make_preferred().string()) + " " +
        addQuotes((Cache::configureDirectory.directoryPath / "file.txt").make_preferred().string()));
    fun.postBuild.emplace_back("sleep 3");
    fun.postBuild.emplace_back(
        "rm " + addQuotes((Cache::configureDirectory.directoryPath / "file.txt").make_preferred().string()));
#endif
    variantRelease.executables.push_back(fun);
    project.projectVariants.push_back(variantRelease);
    project.configure();
    Cache::registerCacheVariables();
}