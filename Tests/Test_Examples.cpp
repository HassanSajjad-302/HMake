
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
    current_path("app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Hello World\n");
}

TEST(ExamplesTest, Example2)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example2"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("Debug/app");
    ExamplesTestHelper::runAppWithExpectedOutput(getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "func1 called\nfunc2 called\nfunc3 called\nfunc4 called\n");
    current_path("../../Release/app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "func1 called\nfunc2 called\nfunc3 called\nfunc4 called\n");
}

TEST(ExamplesTest, Example3)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example3"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("app");
    ExamplesTestHelper::runAppWithExpectedOutput(getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "func() from file1.cpp called.\n");

    Json cacheFileJson;
    current_path("../");
    ifstream("cache.hmake") >> cacheFileJson;
    bool file1 = cacheFileJson.at("cache-variables").get<Json>().at("FILE1").get<bool>();
    ASSERT_EQ(file1, true) << "Cache does not has the Cache-Variable or this variable is not of right value";
    cacheFileJson["cache-variables"]["FILE1"] = false;
    ofstream("cache.hmake") << cacheFileJson.dump(4);

    ASSERT_EQ(system((hhelperStr + " --configure").c_str()), 0) << (hhelperStr + " --configure") + " command failed.";
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";

    current_path("app");
    ExamplesTestHelper::runAppWithExpectedOutput(getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "func() from file2.cpp called.\n");
}

TEST(ExamplesTest, Example4)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example4"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("Animal-Shared/");
    ExamplesTestHelper::runAppWithExpectedOutput(
        getActualNameFromTargetName(TargetType::EXECUTABLE, os, "Animal-Shared"), "Cat says Meow..\n");
    current_path("../Animal-Static/");
    ExamplesTestHelper::runAppWithExpectedOutput(
        getActualNameFromTargetName(TargetType::EXECUTABLE, os, "Animal-Static"), "Cat says Meow..\n");
}

TEST(ExamplesTest, Example5)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example5"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"), "");
}

/*
TEST(ExamplesTest, Example6)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example6"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/Animal/");
    ExamplesTestHelper::runAppWithExpectedOutput(getActualNameFromTargetName(TargetType::EXECUTABLE, os, "Animal"), "Cat
says Meow..\n");
}
*/

#ifdef _WIN32
TEST(ExamplesTest, Example9)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example9"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Hello World\n");
}

TEST(ExamplesTest, Example10)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example10/ball_pit"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
}
#endif
