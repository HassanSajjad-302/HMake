

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
        r.startProcess("hhelper", false);
        const auto &[output, exitStatus] = r.endProcess(false);
        ASSERT_EQ(exitStatus, EXIT_SUCCESS) << FORMAT("First hhelper failed with output\n{}\n.", output);
    }

    {
        RunCommand r;
        r.startProcess("hhelper", false);
        const auto &[output, exitStatus] = r.endProcess(false);
        ASSERT_EQ(exitStatus, EXIT_SUCCESS) << FORMAT("Second hhelper failed with output\n{}\n.", output);
    }

    {
        RunCommand r;
        r.startProcess("hbuild", false);
        const auto &[output, exitStatus] = r.endProcess(false);
        ASSERT_EQ(exitStatus, EXIT_SUCCESS) << FORMAT("hbuild failed with output\n{}\n.", output);
    }
}

void ExamplesTestHelper::runAppWithExpectedOutput(const string &appName, const string &expectedOutput)
{
    RunCommand run;
    run.startProcess(appName, false);
    auto [output, exitStatus] = run.endProcess(false);
    erase_if(output, [](const char c) { return c == '\r'; });
    ASSERT_EQ(exitStatus, EXIT_SUCCESS) << FORMAT("Running {} failed\n. Error {}\n", appName, exitStatus);
    ASSERT_EQ(output, expectedOutput) << FORMAT("Running {} produced unexpected output\n", appName);
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
        r.startProcess("hhelper", false);
        const auto &[_, exitStatus2] = r.endProcess(false);
        ASSERT_EQ(exitStatus2, EXIT_SUCCESS) << FORMAT("First hhelper failed with output\n{}\n.", _);
    }

    {
        RunCommand r;
        r.startProcess("hhelper", false);
        const auto &[_, exitStatus2] = r.endProcess(false);
        ASSERT_EQ(exitStatus2, EXIT_SUCCESS) << FORMAT("Second hhelper failed with output\n{}\n.", _);
    }

    {
        RunCommand r;
        r.startProcess("hbuild", false);
        auto [output2, exitStatus2] = r.endProcess(false);
        output = std::move(output2);
        erase_if(output, [](const char c) { return c == '\r'; });
        exitStatus = exitStatus2;
    }
}

void ExamplesTestHelper::runCommandAndGetOutput(const string &command, string &output)
{

    RunCommand r;
    r.startProcess(command, false);
    auto [output2, exitStatus] = r.endProcess(false);
    ASSERT_EQ(exitStatus, EXIT_SUCCESS) << "Could Not Run " << command;
    output = std::move(output2);
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