

#include "TestHelper.hpp"
#include "filesystem"
#include "fstream"

using std::filesystem::create_directory, std::filesystem::current_path, std::ofstream, std::ifstream, std::stringstream,
    std::filesystem::path;

string TestHelper::runHMakeProject()
{
    if (exists(path("Build")))
    {
        remove_all(path("Build"));
    }
    create_directory("Build");
    current_path("Build");
    system("hhelper.exe");
    system("hhelper.exe");
    system("hbuild.exe");
    current_path("0/app/");
#ifdef _WIN32
    system("app.exe > file");
#else
    system("./app > file");
#endif
    stringstream output;
    output << ifstream("file").rdbuf();
    return output.str();
}

void TestHelper::recreateSourceAndBuildDir()
{
    current_path(TESTDIRECTORY);
    if (exists(path("Source")))
    {
        remove_all(path("Source"));
    }
    create_directory("Source");
    if (exists(path("Build")))
    {
        remove_all(path("Build"));
    }
    create_directory("Build");
}