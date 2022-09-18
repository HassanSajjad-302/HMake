
#include "TestHelper.hpp"
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

TEST(CompilationTest, Example1)
{
    current_path(path(EXAMPLES_DIRECTORY) / path("Example1"));
    TestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/app/");
    TestHelper::runAppWithExpectedOutput(getExeName("app"), "Hello World\n");
}

TEST(CompilationTest, Example2)
{
    current_path(path(EXAMPLES_DIRECTORY) / path("Example2"));
    TestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/Animal/");
    TestHelper::runAppWithExpectedOutput(getExeName("Animal"), "Cat says Meow..\n");
    current_path("../../1/Animal/");
    TestHelper::runAppWithExpectedOutput(getExeName("Animal"), "Cat says Meow..\n");
}

TEST(CompilationTest, Example3)
{
    current_path(path(EXAMPLES_DIRECTORY) / path("Example3"));
    TestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/app");
    TestHelper::runAppWithExpectedOutput(getExeName("app"), "func1 called\nfunc2 called\nfunc3 called\nfunc4 called\n");
    current_path("../../1/app/");
    TestHelper::runAppWithExpectedOutput(getExeName("app"), "func1 called\nfunc2 called\nfunc3 called\nfunc4 called\n");
}

TEST(CompilationTest, Example4)
{
    current_path(path(EXAMPLES_DIRECTORY) / path("Example4"));
    TestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/app");
    TestHelper::runAppWithExpectedOutput(getExeName("app"), "func() from file1.cpp called.\n");

    Json cacheFileJson;
    current_path("../../");
    ifstream("cache.hmake") >> cacheFileJson;
    bool file1 = cacheFileJson.at("cache-variables").get<Json>().at("FILE1").get<bool>();
    ASSERT_EQ(file1, true) << "Cache does not has the Cache-Variable or this variable is not of right value";
    cacheFileJson["cache-variables"]["FILE1"] = false;
    ofstream("cache.hmake") << cacheFileJson.dump(4);

    ASSERT_EQ(system(getSlashedExeName("configure").c_str()), 0) << getExeName("configure") + " command failed.";
    ASSERT_EQ(system(getExeName("hbuild").c_str()), 0) << getExeName("hbuild") + " command failed.";

    current_path("0/app");
    TestHelper::runAppWithExpectedOutput(getExeName("app"), "func() from file2.cpp called.\n");
}

TEST(CompilationTest, Example5)
{
    current_path(path(EXAMPLES_DIRECTORY) / path("Example5"));
    TestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/app/");
    TestHelper::runAppWithExpectedOutput(getExeName("app"), "");
}

TEST(CompilationTest, Example6)
{
    current_path(path(EXAMPLES_DIRECTORY) / path("Example6"));
    TestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/Animal/");
    TestHelper::runAppWithExpectedOutput(getExeName("Animal"), "Cat says Meow..\n");
}

TEST(CompilationTest, Example7)
{
    current_path(path(EXAMPLES_DIRECTORY) / path("Example7"));
    TestHelper::recreateBuildDirAndBuildHMakeProject(true);
}

TEST(CompilationTest, Example8)
{
    current_path(path(EXAMPLES_DIRECTORY) / path("Example8"));
    TestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/Pets/");
    TestHelper::runAppWithExpectedOutput(getExeName("Pets"), "Dog says woof\nCat says Meow..\nGoat says baa\n");
    current_path("../../1/Pets/");
    TestHelper::runAppWithExpectedOutput(getExeName("Pets"), "Dog says woof\nCat says Meow..\nGoat says baa\n");
}

#ifdef _WIN32
TEST(CompilationTest, Example9)
{
    current_path(path(EXAMPLES_DIRECTORY) / path("Example9"));
    TestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/app/");
    TestHelper::runAppWithExpectedOutput(getExeName("app"), "Hello World\n");
}

TEST(CompilationTest, Example10)
{
    current_path(path(EXAMPLES_DIRECTORY) / path("Example10/ball_pit"));
    TestHelper::recreateBuildDirAndBuildHMakeProject();
}
#endif

int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
