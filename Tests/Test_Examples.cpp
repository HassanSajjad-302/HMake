
#include "BuildSystemFunctions.hpp"
#include "ExamplesTestHelper.hpp"
#include "Features.hpp"
#include "filesystem"
#include "fmt/format.h"
#include "fstream"
#include "iostream"
#include "nlohmann/json.hpp"
#include "string"
#include "gtest/gtest.h"

using std::string, std::ofstream, std::ifstream, std::filesystem::create_directory, std::filesystem::path,
    std::filesystem::current_path, std::cout, fmt::format, std::filesystem::remove_all, std::ifstream, std::ofstream;

using Json = nlohmann::ordered_json;

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
    bool file1 = cacheFileJson.at("cache-variables").get<Json>().at("FILE1").get<bool>();
    ASSERT_EQ(file1, true) << "Cache does not has the Cache-Variable or this variable is not of right value";
    cacheFileJson["cache-variables"]["FILE1"] = false;
    ofstream("cache.json") << cacheFileJson.dump(4);

    ASSERT_EQ(system((hhelperStr + " --configure").c_str()), 0) << (hhelperStr + " --configure") + " command failed.";
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
#endif
