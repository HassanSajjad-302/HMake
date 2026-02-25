

#include "ExamplesTestHelper.hpp"

#include "RunCommand.hpp"

#include "gtest/gtest.h"
#include <filesystem>
#include <fstream>
#include <iostream>

using std::cout, std::endl, std::filesystem::create_directory, std::filesystem::current_path, std::ofstream,
    std::ifstream, std::stringstream, std::filesystem::path;

void ExamplesTestHelper::cleanBuild()
{
    if (exists(path("Build")))
    {
        for (const auto &entry : std::filesystem::directory_iterator("Build"))
            std::filesystem::remove_all(entry.path());
    }
    create_directory("Build");
    current_path("Build");

    {
        RunCommand r;
        r.runProcess("hhelper");
        ASSERT_EQ(r.exitStatus, EXIT_SUCCESS) << FORMAT("First hhelper failed with output\n{}\n.", r.output);
    }

    {
        RunCommand r;
        r.runProcess("hhelper");
        ASSERT_EQ(r.exitStatus, EXIT_SUCCESS) << FORMAT("Second hhelper failed with output\n{}\n.", r.output);
    }

    {
        RunCommand r;
        r.runProcess("hbuild");
        ASSERT_EQ(r.exitStatus, EXIT_SUCCESS) << FORMAT("hbuild failed with output\n{}\n.", r.output);
    }
}

void ExamplesTestHelper::runAppWithExpectedOutput(const string &appName, const string &expectedOutput)
{
    RunCommand run;
    run.runProcess(appName.c_str());
    erase_if(run.output, [](const char c) { return c == '\r'; });
    ASSERT_EQ(run.exitStatus, EXIT_SUCCESS) << FORMAT("Running {} failed\n. Error {}\n", appName, run.exitStatus);
    ASSERT_EQ(run.output, expectedOutput) << FORMAT("Running {} produced unexpected output\n", appName);
}

void ExamplesTestHelper::getCleanBuildOutputAndStatus(string &output, int32_t &exitStatus)
{
    if (exists(path("Build")))
    {
        remove_all(path("Build"));
    }
    create_directory("Build");
    current_path("Build");

    {
        RunCommand r;
        r.runProcess("hhelper");
        ASSERT_EQ(r.exitStatus, EXIT_SUCCESS) << FORMAT("First hhelper failed with output\n{}\n.", r.output);
    }

    {
        RunCommand r;
        r.runProcess("hhelper");
        ASSERT_EQ(r.exitStatus, EXIT_SUCCESS) << FORMAT("Second hhelper failed with output\n{}\n.", r.output);
    }

    {
        RunCommand r;
        r.runProcess("hbuild");
        erase_if(r.output, [](const char c) { return c == '\r'; });
        exitStatus = r.exitStatus;
        output = std::move(r.output);
    }

}

void ExamplesTestHelper::runCommandAndGetOutput(const string &command, string &output)
{

    RunCommand r;
    r.runProcess(command.c_str());
    ASSERT_EQ(r.exitStatus, EXIT_SUCCESS) << "Could Not Run " << command;
    output = r.output;
    erase_if(output, [](const char c) { return c == '\r'; });
}

void ExamplesTestHelper::getCommandOutputInDir(const string &dir, const string &command, string &output)
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