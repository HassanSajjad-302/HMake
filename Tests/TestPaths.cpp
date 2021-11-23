

#include "TestPaths.hpp"
#include "filesystem"
#include "fstream"
#include "iostream"
#include "streambuf"

using std::filesystem::create_directory, std::filesystem::current_path, std::ofstream, std::ifstream, std::stringstream;

string TestPaths::compileAndRunHMakeProject(const string &hmakeFile, const string &mainSourceFile)
{
    std::filesystem::current_path(TESTDIRECTORY);
    create_directory("Source");
    current_path("Source");
    ofstream("hmake.cpp") << hmakeFile;
    ofstream("main.cpp") << mainSourceFile;
    if (exists(path("Build")))
    {
        remove_all(path("Build"));
        create_directory("Build");
    }
    else
    {
        create_directory("Build");
    }
    current_path("Build");
    system("hhelper.exe");
    system("hhelper.exe");
    system("hbuild.exe");
    current_path("0/app/");
    system("app.exe > file");
    stringstream output;
    output << ifstream("file").rdbuf();
    return output.str();
}
