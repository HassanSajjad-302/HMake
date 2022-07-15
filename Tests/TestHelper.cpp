

#include "TestHelper.hpp"
#include "filesystem"
#include "fstream"
#include "gtest/gtest.h"
#include <iostream>

using std::cout, std::endl, std::filesystem::create_directory, std::filesystem::current_path, std::ofstream,
    std::ifstream, std::stringstream, std::filesystem::path;

void TestHelper::recreateBuildDirAndBuildHMakeProject(bool onlyPackageNoProject)
{
    if (std::filesystem::exists(path("Build")))
    {
        remove_all(path("Build"));
    }
    create_directory("Build");
    current_path("Build");
    ASSERT_EQ(system(getExeName("hhelper").c_str()), 0) << "First " + getExeName("hhelper") + " command failed.";
    ASSERT_EQ(system(getExeName("hhelper").c_str()), 0) << "Second " + getExeName("hhelper") + " command failed.";
    if (std::filesystem::exists("Package"))
    {
        current_path("Package");
        ASSERT_EQ(system(getExeName("hbuild").c_str()), 0) << getExeName("hbuild") + " command failed.";
        current_path("../");
    }
    if (!onlyPackageNoProject)
    {
        ASSERT_EQ(system(getExeName("hbuild").c_str()), 0) << getExeName("hbuild") + " command failed.";
    }
}

void TestHelper::runAppWithExpectedOutput(const string &appName, const string &expectedOutput)
{
    const string command = getSlashedExeName(appName) + " > file";
    ASSERT_EQ(system(command.c_str()), 0) << "Could Not Run " << appName;
    stringstream output;
    output << ifstream("file").rdbuf();
    ASSERT_EQ(output.str(), expectedOutput)
        << "hmake build succeeded, however running the application did not produce expected output";
}

void TestHelper::recreateBuildDir()
{
    if (std::filesystem::exists(path("Build")))
    {
        remove_all(path("Build"));
    }
    create_directory("Build");
}