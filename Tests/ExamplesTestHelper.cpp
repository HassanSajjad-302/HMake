

#include "ExamplesTestHelper.hpp"
#include "gtest/gtest.h"
#include <filesystem>
#include <fstream>
#include <iostream>

using std::cout, std::endl, std::filesystem::create_directory, std::filesystem::current_path, std::ofstream,
    std::ifstream, std::stringstream, std::filesystem::path;

void ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject()
{
    if (exists(path("Build")))
    {
        remove_all(path("Build"));
    }
    create_directory("Build");
    current_path("Build");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << "First " + hhelperStr + " command failed.";
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << "Second " + hhelperStr + " command failed.";
    const int exitCode = system(hbuildBuildStr.c_str());
    ASSERT_EQ(exitCode, 0) << hbuildBuildStr + " command failed.";
}

void ExamplesTestHelper::runAppWithExpectedOutput(const string &appName, const string &expectedOutput)
{
    const string command = appName + " > file";
    ASSERT_EQ(system(command.c_str()), 0) << "Could Not Run " << appName;
    stringstream output;
    output << ifstream("file").rdbuf();
    ASSERT_EQ(output.str(), expectedOutput)
        << "hmake build succeeded, however running the application did not produce expected output";
}

void ExamplesTestHelper::recreateBuildDirAndGethbuildOutput(string &output, int32_t exitStatus)
{
    if (exists(path("Build")))
    {
        remove_all(path("Build"));
    }
    create_directory("Build");
    current_path("Build");

    ASSERT_EQ(system(hhelperStr.c_str()), 0) << "First " + hhelperStr + " command failed.";
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << "Second " + hhelperStr + " command failed.";

    const string command = "hbuild > file 2>&1 ";
    ASSERT_EQ(system(command.c_str()), exitStatus) << "Could Not Run " << command;
    stringstream outputStream;
    outputStream << ifstream("file").rdbuf();
    output = outputStream.str();
}

void ExamplesTestHelper::runCommandAndGetOutput(const string &command, string &output)
{
    const string fullCommand = command + " > file 2>&1 ";
    ASSERT_EQ(system(fullCommand.c_str()), EXIT_SUCCESS) << "Could Not Run " << command;
    stringstream outputStream;
    outputStream << ifstream("file").rdbuf();
    output = outputStream.str();
}

void ExamplesTestHelper::runCommandAndGetOutputInDirectory(const string &dir, const string &command, string &output)
{
    const path p = current_path();
    current_path(dir);
    runCommandAndGetOutput(command, output);
    current_path(p);
}

void ExamplesTestHelper::recreateBuildDir()
{
    if (exists(path("Build")))
    {
        remove_all(path("Build"));
    }
    create_directory("Build");
}