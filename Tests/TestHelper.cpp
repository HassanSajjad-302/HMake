

#include "gtest/gtest.h"
#include <iostream>
using std::cout, std::endl;
#include "TestHelper.hpp"
#include "filesystem"
#include "fstream"

using std::filesystem::create_directory, std::filesystem::current_path, std::ofstream, std::ifstream, std::stringstream,
    std::filesystem::path;

void TestHelper::runHMakeProjectWithExpectedOutput(const string &expectedOutput)
{
    if (exists(path("Build")))
    {
        remove_all(path("Build"));
    }
    create_directory("Build");
    current_path("Build");
    ASSERT_EQ(system("hhelper.exe"), 0) << "First hhelper.exe command failed.";
    ASSERT_EQ(system("hhelper.exe"), 0) << "Second hhelper.exe command failed.";
    ASSERT_EQ(system("hbuild.exe"), 0) << "hbuild.exe command failed.";
    current_path("0/app/");
#ifdef _WIN32
    system("app.exe > file");
#else
    system("./app > file");
#endif
    stringstream output;
    output << ifstream("file").rdbuf();
    ASSERT_EQ(output.str(), expectedOutput)
        << "hmake build succeeded, however running the application did not produce expected output";
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