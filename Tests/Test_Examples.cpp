
#include "BuildSystemFunctions.hpp"
#include "ExamplesTestHelper.hpp"
#include "Features.hpp"
#include "gtest/gtest.h"
#include <fstream>
#include <regex>

using std::string, std::ofstream, std::ifstream, std::filesystem::create_directory, std::filesystem::path,
    std::filesystem::current_path, std::cout, std::format, std::filesystem::remove_all, std::ifstream, std::ofstream;

TEST(ExamplesTest, Example1)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example1"));
    ExamplesTestHelper::cleanBuild();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/Release/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Hello World\n");
}

#ifdef _WIN32
TEST(ExamplesTest, Example2)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example2"));
    ExamplesTestHelper::cleanBuild();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/Debug/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "func1 called\nfunc2 called\nfunc3 called\nfunc4 called\n");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/Release/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "func1 called\nfunc2 called\nfunc3 called\nfunc4 called\n");
}
#endif

TEST(ExamplesTest, Example3)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example3"));
    ExamplesTestHelper::cleanBuild();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/Release/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "func() from file1.cpp called.\n");

    ifstream ifs("cache.txt");
    string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    const uint64_t variable = content.find("FILE1=true");
    ASSERT_NE(variable, string::npos);
    content.replace(variable, string_view("FILE1=true").size(), "FILE1=false");
    {
        ofstream ofs("cache.txt");
        ofs << content;
    }

    ASSERT_EQ(system(hconfigureOnlyStr.c_str()), 0) << hconfigureOnlyStr + " command failed.";
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";

    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/Release/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "func() from file2.cpp called.\n");
}

TEST(ExamplesTest, Example4)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example4"));
    ExamplesTestHelper::cleanBuild();
    ExamplesTestHelper::runAppWithExpectedOutput(
        current_path().string() + "/Release/Animal-Shared/" +
            getActualNameFromTargetName(TargetType::EXECUTABLE, os, "Animal-Shared"),
        "Cat says Meow..\n");
    ExamplesTestHelper::runAppWithExpectedOutput(
        current_path().string() + "/Release/Animal-Static/" +
            getActualNameFromTargetName(TargetType::EXECUTABLE, os, "Animal-Static"),
        "Cat says Meow..\n");
}

TEST(ExamplesTest, Example6)
{

    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example6"));
    ExamplesTestHelper::cleanBuild();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/Release/App-Static/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Cat says Meow..\nDog says Woof..\n");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/Release/App-Shared/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Cat says Meow..\nDog says Woof..\n");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/Release/App2-Static/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Cat says Meow..\nDog says Woof..\n");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/Release/App2-Shared/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Cat says Meow..\nDog says Woof..\n");
    ExamplesTestHelper::runAppWithExpectedOutput(
        current_path().string() + "/Release/App-MixedPrivate/" +
            getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
        "Cat says Meow..\nDog says Woof..\n");
    ExamplesTestHelper::runAppWithExpectedOutput(
        current_path().string() + "/Release/App-MixedInterface/" +
            getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
        "Cat says Meow..\n");
}

TEST(ExamplesTest, Example7)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example7"));
    ExamplesTestHelper::cleanBuild();
    /*ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/modules/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Hello World\n");*/
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/hu/app2/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app2"),
                                                 "Hello World\n");
}

TEST(ExamplesTest, Example8)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example8"));
    ExamplesTestHelper::cleanBuild();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/Release/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Hello World\n");
}

TEST(ExamplesTest, Example9)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example9"));
    ExamplesTestHelper::cleanBuild();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/static/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "36\n");
}

#ifdef _WIN32
TEST(ExamplesTest, Example10)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example10"));
    ExamplesTestHelper::cleanBuild();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/Release/appA/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "appA"),
                                                 "My Name is Library A\nMy Name is Library B\n");
}
#endif

TEST(AExamplesTest, Example_A1)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example-A1"));
    string output;
    int exitStatus;
    ExamplesTestHelper::getCleanBuildOutputAndStatus(output, exitStatus);
    ASSERT_EQ(output, "Hello\nWorld\n");
    ASSERT_EQ(exitStatus, EXIT_SUCCESS);
}

TEST(AExamplesTest, Example_A2)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example-A2"));
    string output;
    int exitStatus;
    ExamplesTestHelper::getCleanBuildOutputAndStatus(output, exitStatus);
    ASSERT_EQ(exitStatus, EXIT_SUCCESS);
    ASSERT_EQ(output, "World\nHello\nHello\nWorld\n");
}

std::string removeColorCodes(const std::string &str)
{
    // Regex pattern to match ANSI escape sequences
    std::regex colorCodeRegex("\x1B\\[[0-9;]*m");
    // Replace all occurrences of the pattern with an empty string
    return std::regex_replace(str, colorCodeRegex, "");
}

TEST(AExamplesTest, Example_A4)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example-A4"));
    string output;
    int exitStatus;
    ExamplesTestHelper::getCleanBuildOutputAndStatus(output, exitStatus);
    ASSERT_EQ(exitStatus, EXIT_FAILURE);
    string str = "error: Dependency graph contains a cycle.\nCycle: Cat1 -> Cat2 -> Cat3 -> Cat1\n";
    string result = removeColorCodes(output);
    ASSERT_EQ(result, str);
}

TEST(AExamplesTest, Example_A5)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example-A5"));
    string output;
    int exitStatus;
    ExamplesTestHelper::getCleanBuildOutputAndStatus(output, exitStatus);
    ASSERT_EQ(exitStatus, EXIT_FAILURE);
    const string str = R"(Hello
World
Target Ninja runtime error.
HMake
XMake
Target build2 runtime error.
)";
    const vector<string_view> expected = split(str, '\n');
    const vector<string_view> actual = split(output, '\n');
    ASSERT_EQ(expected.size(), actual.size());
    for (const string_view &s : actual)
    {
        bool found = false;
        for (const string_view &c : expected)
        {
            if (s == c)
            {
                found = true;
            }
        }
        ASSERT_EQ(found, true);
    }
}

TEST(AExamplesTest, Example_A6)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example-A6"));
    string output;
    int exitStatus;
    ExamplesTestHelper::getCleanBuildOutputAndStatus(output, exitStatus);
    ASSERT_EQ(exitStatus, EXIT_SUCCESS);
    constexpr uint64_t count = 60 * 2 + 200 * 3 + 260;
    ASSERT_EQ(output.size(), count);
    string sub(output.begin() + 400, output.begin() + 407);
    ASSERT_EQ(sub, "900 901");
}

TEST(AExamplesTest, Example_A7)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example-A7"));
    string output;
    int exitStatus;
    ExamplesTestHelper::getCleanBuildOutputAndStatus(output, exitStatus);
    ASSERT_EQ(exitStatus, EXIT_FAILURE);
    string str = "error: Dependency graph contains a cycle.\nCycle: b -> c -> b\n";
    string result = removeColorCodes(output);
    ASSERT_EQ(result, str);
}

TEST(AExamplesTest, Example_A9)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example-A9"));
    string output;
    int exitStatus;
    ExamplesTestHelper::getCleanBuildOutputAndStatus(output, exitStatus);
    ASSERT_EQ(exitStatus, EXIT_SUCCESS);
    ASSERT_EQ(output.size(), 5);
    ASSERT_EQ(output.contains('D'), true);
    ASSERT_EQ(output.contains('E'), true);
    ASSERT_EQ(output.contains('A'), true);
    ASSERT_EQ(output.contains('B'), true);
    ASSERT_EQ(output.contains('F'), true);
    ExamplesTestHelper::getCommandOutputInDir("D", "hbuild D", output);
    ASSERT_EQ(output == "D", true);
    ExamplesTestHelper::getCommandOutputInDir("E", "hbuild e", output);
    ASSERT_EQ(output == "E", true);
    ExamplesTestHelper::getCommandOutputInDir("A", "hbuild", output);
    ASSERT_EQ(output.size(), 2);
    ASSERT_EQ(output.contains('A'), true);
    ASSERT_EQ(output.contains('B'), true);

    ExamplesTestHelper::runCommandAndGetOutput("hbuild A/C", output);
    ASSERT_EQ(output.size(), 6);
    ASSERT_EQ(output.contains('A'), true);
    ASSERT_EQ(output.contains('B'), true);
    ASSERT_EQ(output.contains('E'), true);
    ASSERT_EQ(output.contains('C'), true);
    ASSERT_EQ(output.contains('D'), true);
    ASSERT_EQ(output.contains('F'), true);

    ExamplesTestHelper::getCommandOutputInDir("A/C", "hbuild .", output);
    ASSERT_EQ(output.size(), 3);
    ASSERT_EQ(output.contains('A'), true);
    ASSERT_EQ(output.contains('E'), true);
    ASSERT_EQ(output.contains('C'), true);

    ExamplesTestHelper::getCommandOutputInDir("A", "hbuild C", output);
    ASSERT_EQ(output.size(), 4);
    ASSERT_EQ(output.contains('A'), true);
    ASSERT_EQ(output.contains('E'), true);
    ASSERT_EQ(output.contains('C'), true);
    ASSERT_EQ(output.contains('B'), true);

    ExamplesTestHelper::getCommandOutputInDir("A", "hbuild C ../D", output);
    ASSERT_EQ(output.size(), 5);
    ASSERT_EQ(output.contains('A'), true);
    ASSERT_EQ(output.contains('E'), true);
    ASSERT_EQ(output.contains('C'), true);
    ASSERT_EQ(output.contains('B'), true);
    ASSERT_EQ(output.contains('D'), true);

    // ASSERT_EQ(output.contains('D'), true);
}

TEST(AExamplesTest, Example_A10)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example-A10"));
    string output;
    int exitStatus;

    if (exists(path("Build")))
    {
        remove_all(path("Build"));
    }
    create_directory("Build");
    current_path("Build");

    {
        const auto result = RunCommand::runProcess(hconfigureOnlyStr);
        ASSERT_EQ(result.exitStatus, EXIT_SUCCESS)
            << FORMAT("hbuild configuration failed with output\n{}\n.", result.output);
    }

    {
        ASSERT_EQ(system("c++ ../main.cpp"), EXIT_SUCCESS) << "c++ ../main.cpp failed\n";
    }

    {
        auto result = RunCommand::runProcess("hbuild");
        erase_if(result.output, [](const char c) { return c == '\r'; });
        exitStatus = result.exitStatus;
        output = std::move(result.output);
    }

    ASSERT_EQ(exitStatus, EXIT_SUCCESS);

    const string str =
        "\x1B[38;2;255;165;0m./a.out \xE2\x86\x92 build-system message:\n\x1B[0mFirst message to build-system: this "
        "module depends on 'std'. Please provide it.\n\x1B[38;2;255;165;0m./a.out \xE2\x86\x92 build-system "
        "message:\n\x1B[0mFinal message to build-system: compilation finished.\n\x1B[38;2;144;238;144m./a.out finished "
        "successfully:\n\x1B[0mHello World\nModule received: std\nYey\n";
    ASSERT_EQ(str, output);
}
