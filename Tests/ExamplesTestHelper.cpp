

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
        {
            std::filesystem::remove_all(entry.path());
        }
    }
    create_directory("Build");
    current_path("Build");

    {
        const auto result = RunCommand::runProcess("hbuild");
        ASSERT_EQ(result.exitStatus, EXIT_SUCCESS) << FORMAT("hbuild failed with output\n{}\n.", result.output);
    }
}

void ExamplesTestHelper::runAppWithExpectedOutput(const string &appName, const string &expectedOutput,
                                                  const char *workingDirectory)
{
    STACK_PMR_STRING(command, 4 * 1024)
    command += '"';
    for (const char character : appName)
    {
        if constexpr (os != OS::NT)
        {
            if (character == '\\' || character == '"')
            {
                command += '\\';
            }
        }
        command += character;
    }
    command += '"';
    auto result = RunCommand::runProcess(command, workingDirectory);
    erase_if(result.output, [](const char c) { return c == '\r'; });
    ASSERT_EQ(result.exitStatus, EXIT_SUCCESS) << FORMAT("Running {} failed\n. Error {}\n", appName, result.exitStatus);
    ASSERT_EQ(result.output, expectedOutput) << FORMAT("Running {} produced unexpected output\n", appName);
}

void ExamplesTestHelper::filterBootstrapMessages(string &output)
{
    const uint64_t pos = output.find("configure execution time:");
    if (pos != string::npos)
    {
        const uint64_t endOfLine = output.find('\n', pos);
        output.erase(0, endOfLine != string::npos ? endOfLine + 1 : output.size());
    }
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
        filterBootstrapMessages(output);
    }
}

void ExamplesTestHelper::runCommandAndGetOutput(const string &command, string &output)
{
    auto result = RunCommand::runProcess(command);
    ASSERT_EQ(result.exitStatus, EXIT_SUCCESS) << "Could Not Run " << command;
    output = std::move(result.output);
    erase_if(output, [](const char c) { return c == '\r'; });
    filterBootstrapMessages(output);
}

void ExamplesTestHelper::getCommandOutputInDir(const string &dir, const string &command, string &output)
{
    auto result = RunCommand::runProcess(command, dir.c_str());
    ASSERT_EQ(result.exitStatus, EXIT_SUCCESS) << "Could Not Run " << command;
    output = std::move(result.output);
    erase_if(output, [](const char c) { return c == '\r'; });
    filterBootstrapMessages(output);
}

void ExamplesTestHelper::recreateBuildDir()
{
    if (exists(path("Build")))
    {
        remove_all(path("Build"));
    }
    create_directory("Build");
}
