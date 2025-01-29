
#include "BuildSystemFunctions.hpp"
#include "ExamplesTestHelper.hpp"
#include "Features.hpp"
#include "Utilities.hpp"
#include "fmt/format.h"
#include "nlohmann/json.hpp"
#include "gtest/gtest.h"
#include <fstream>
#include <regex>

using std::string, std::ofstream, std::ifstream, std::filesystem::create_directory, std::filesystem::path,
    std::filesystem::current_path, std::cout, fmt::format, std::filesystem::remove_all, std::ifstream, std::ofstream;

TEST(ExamplesTest, Example1)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example1"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Hello World\n");
}

TEST(ExamplesTest, Example2)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example2"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/Debug/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "func1 called\nfunc2 called\nfunc3 called\nfunc4 called\n");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/Release/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "func1 called\nfunc2 called\nfunc3 called\nfunc4 called\n");
}

TEST(ExamplesTest, Example3)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example3"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "func() from file1.cpp called.\n");

    Json cacheFileJson;
    ifstream("cache.json") >> cacheFileJson;
    const bool file1 = cacheFileJson.at("cache-variables").get<Json>().at("FILE1").get<bool>();
    ASSERT_EQ(file1, true) << "Cache does not has the Cache-Variable or this variable is not of right value";
    cacheFileJson["cache-variables"]["FILE1"] = false;
    ofstream("cache.json") << cacheFileJson.dump(4);

    ASSERT_EQ(system((hhelperStr + " --configure").c_str()), 0) << hhelperStr + " --configure" + " command failed.";
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";

    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "func() from file2.cpp called.\n");
}

TEST(ExamplesTest, Example4)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example4"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    ExamplesTestHelper::runAppWithExpectedOutput(
        current_path().string() + "/Animal-Shared/" +
            getActualNameFromTargetName(TargetType::EXECUTABLE, os, "Animal-Shared"),
        "Cat says Meow..\n");
    ExamplesTestHelper::runAppWithExpectedOutput(
        current_path().string() + "/Animal-Static/" +
            getActualNameFromTargetName(TargetType::EXECUTABLE, os, "Animal-Static"),
        "Cat says Meow..\n");
}

#ifndef WIN32
TEST(ExamplesTest, Example5)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example5"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/Animal/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "Animal"),
                                                 "Cat says Meow..\n");
}
#endif

TEST(ExamplesTest, Example6)
{

    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example6"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/App-Static/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Cat says Meow..\nDog says Woof..\n");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/App-Shared/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Cat says Meow..\nDog says Woof..\n");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/App2-Static/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Cat says Meow..\nDog says Woof..\n");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/App2-Shared/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Cat says Meow..\nDog says Woof..\n");
}

#ifdef _WIN32
TEST(ExamplesTest, Example7)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example7"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Hello World\n");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/app2/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app2"),
                                                 "Hello World\n");
}

TEST(ExamplesTest, Example8)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example8"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Hello World\n");
}

TEST(ExamplesTest, Example9)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example9"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/static/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "36\n");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/object/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "36\n");
}
#endif

TEST(AExamplesTest, Example_A1)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example-A1"));
    string output;
    ExamplesTestHelper::recreateBuildDirAndGethbuildOutput(output, EXIT_SUCCESS);
    ASSERT_EQ(output, "Hello\nWorld\n");
}

TEST(AExamplesTest, Example_A2)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example-A2"));
    string output;
    ExamplesTestHelper::recreateBuildDirAndGethbuildOutput(output, EXIT_SUCCESS);
    ASSERT_EQ(output, "World\nHello\nHello\nWorld\n");
}

TEST(AExamplesTest, Example_A3)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example-A3"));
    string output;
    ExamplesTestHelper::recreateBuildDirAndGethbuildOutput(output, EXIT_SUCCESS);
    constexpr uint64_t count = 30 * 2 + 200 * 3 + 230;
    ASSERT_EQ(output.size(), count);
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
    ExamplesTestHelper::recreateBuildDirAndGethbuildOutput(output, EXIT_FAILURE);
    string str = R"(There is a Cyclic-Dependency.
BTarget 2 Depends On BTarget 1.
BTarget 1 Depends On BTarget 0.
BTarget 0 Depends On BTarget 2.
)";
    string result = removeColorCodes(output);
    ASSERT_EQ(result, str);
}

TEST(AExamplesTest, Example_A5)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example-A5"));
    string output;
    ExamplesTestHelper::recreateBuildDirAndGethbuildOutput(output, EXIT_FAILURE);
    const string str = R"(Hello
World
Target Ninja runtime error.
HMake
XMake
Target build2 runtime error.
)";
    vector<string> expected = split(str, "\n");
    vector<string> actual = split(output, "\n");
    ASSERT_EQ(expected.size(), actual.size());
    for (string &s : actual)
    {
        bool found = false;
        for (string &c : expected)
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
    ExamplesTestHelper::recreateBuildDirAndGethbuildOutput(output, EXIT_SUCCESS);
    constexpr uint64_t count = 60 * 2 + 200 * 3 + 260;
    ASSERT_EQ(output.size(), count);
    string sub(output.begin() + 400, output.begin() + 407);
    ASSERT_EQ(sub, "900 901");
}

TEST(AExamplesTest, Example_A7)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example-A7"));
    string output;
    ExamplesTestHelper::recreateBuildDirAndGethbuildOutput(output, EXIT_FAILURE);
    string str = R"(There is a Cyclic-Dependency.
BTarget 1 Depends On BTarget 0.
BTarget 0 Depends On BTarget 1.
)";
    string result = removeColorCodes(output);
    ASSERT_EQ(result, str);
}

TEST(AExamplesTest, Example_A9)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example-A9"));
    string output;
    ExamplesTestHelper::recreateBuildDirAndGethbuildOutput(output, EXIT_SUCCESS);
    ASSERT_EQ(output.size(), 5);
    ASSERT_EQ(output.contains('D'), true);
    ASSERT_EQ(output.contains('E'), true);
    ASSERT_EQ(output.contains('A'), true);
    ASSERT_EQ(output.contains('B'), true);
    ASSERT_EQ(output.contains('F'), true);
    ExamplesTestHelper::runCommandAndGetOutputInDirectory("D", "hbuild D", output);
    ASSERT_EQ(output == "D", true);
    ExamplesTestHelper::runCommandAndGetOutputInDirectory("E", "hbuild e", output);
    ASSERT_EQ(output == "E", true);
    ExamplesTestHelper::runCommandAndGetOutputInDirectory("A", "hbuild", output);
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

    ExamplesTestHelper::runCommandAndGetOutputInDirectory("A/C", "hbuild .", output);
    ASSERT_EQ(output.size(), 3);
    ASSERT_EQ(output.contains('A'), true);
    ASSERT_EQ(output.contains('E'), true);
    ASSERT_EQ(output.contains('C'), true);

    ExamplesTestHelper::runCommandAndGetOutputInDirectory("A", "hbuild C", output);
    ASSERT_EQ(output.size(), 4);
    ASSERT_EQ(output.contains('A'), true);
    ASSERT_EQ(output.contains('E'), true);
    ASSERT_EQ(output.contains('C'), true);
    ASSERT_EQ(output.contains('B'), true);

    ExamplesTestHelper::runCommandAndGetOutputInDirectory("A", "hbuild C ../D", output);
    ASSERT_EQ(output.size(), 5);
    ASSERT_EQ(output.contains('A'), true);
    ASSERT_EQ(output.contains('E'), true);
    ASSERT_EQ(output.contains('C'), true);
    ASSERT_EQ(output.contains('B'), true);
    ASSERT_EQ(output.contains('D'), true);

    // ASSERT_EQ(output.contains('D'), true);
}
