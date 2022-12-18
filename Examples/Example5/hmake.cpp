#include "Configure.hpp"

int main()
{
    Cache::initializeCache();
    Project project;
    ProjectVariant variantRelease(project);

    Executable app("app", variantRelease);
    app.SOURCE_FILES("main.cpp");

    // PreBuild commands are executed before the build starts while the PostBuild  commands are executed after
    // the build of the target is finished. Cache::sourceDirectory.path and Cache::configureDirectory.path can be
    //  provided here. Open both srcDir and buildDir in one frame and then run hbuild in terminal to see it.
#ifdef _WIN32
    app.preBuild.emplace_back("echo > " + addQuotes(path((srcDir + "file.txt")).make_preferred().string()));
    app.preBuild.emplace_back("ping 127.0.0.1 -n 2 > nul"); // sleep command windows alternative
    app.postBuild.emplace_back(
        "move " + addQuotes((Cache::sourceDirectory.directoryPath / "file.txt").make_preferred().string()) + " " +
        addQuotes((Cache::configureDirectory.directoryPath / "file.txt").make_preferred().string()));
    app.postBuild.emplace_back("ping 127.0.0.1 -n 3 > nul");
    app.postBuild.emplace_back(
        "del " + addQuotes((Cache::configureDirectory.directoryPath / "file.txt").make_preferred().string()));
#else
    app.preBuild.emplace_back("touch " +
                              addQuotes((Cache::sourceDirectory.directoryPath / "file.txt").make_preferred().string()));
    app.preBuild.emplace_back("sleep 2");
    app.postBuild.emplace_back(
        "mv " + addQuotes((Cache::sourceDirectory.directoryPath / "file.txt").make_preferred().string()) + " " +
        addQuotes((Cache::configureDirectory.directoryPath / "file.txt").make_preferred().string()));
    app.postBuild.emplace_back("sleep 3");
    app.postBuild.emplace_back(
        "rm " + addQuotes((Cache::configureDirectory.directoryPath / "file.txt").make_preferred().string()));
#endif
    project.configure();
    Cache::registerCacheVariables();
}