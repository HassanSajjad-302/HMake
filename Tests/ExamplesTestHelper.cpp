

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
        const auto result = RunCommand::runProcess("hbuild");
        ASSERT_EQ(result.exitStatus, EXIT_SUCCESS) << FORMAT("hbuild failed with output\n{}\n.", result.output);
    }
}

void ExamplesTestHelper::runAppWithExpectedOutput(const string &appName, const string &expectedOutput)
{
    auto result = RunCommand::runProcess(appName);
    erase_if(result.output, [](const char c) { return c == '\r'; });
    ASSERT_EQ(result.exitStatus, EXIT_SUCCESS)
        << FORMAT("Running {} failed\n. Error {}\n", appName, result.exitStatus);
    ASSERT_EQ(result.output, expectedOutput) << FORMAT("Running {} produced unexpected output\n", appName);
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
        auto result = RunCommand::runProcess("hbuild");
        erase_if(result.output, [](const char c) { return c == '\r'; });
        exitStatus = result.exitStatus;
        output = std::move(result.output);
    }

}

void ExamplesTestHelper::runCommandAndGetOutput(const string &command, string &output)
{
    auto result = RunCommand::runProcess(command);
    ASSERT_EQ(result.exitStatus, EXIT_SUCCESS) << "Could Not Run " << command;
    output = std::move(result.output);
    erase_if(output, [](const char c) { return c == '\r'; });
}

void ExamplesTestHelper::getCommandOutputInDir(const string &dir, const string &command, string &output)
{
    const path previousDirectory = current_path();
    current_path(dir);
    auto result = RunCommand::runProcess(command);
    current_path(previousDirectory);
    ASSERT_EQ(result.exitStatus, EXIT_SUCCESS) << "Could Not Run " << command;
    output = std::move(result.output);
    erase_if(output, [](const char c) { return c == '\r'; });
}

void ExamplesTestHelper::recreateBuildDir()
{
    if (exists(path("Build")))
    {
        remove_all(path("Build"));
    }
    create_directory("Build");
}
