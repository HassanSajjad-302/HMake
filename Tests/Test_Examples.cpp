
#include "ExamplesTestHelper.hpp"
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
    current_path("0/app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getExeName("app"), "Hello World\n");
}

TEST(ExamplesTest, Example2)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example2"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/Animal/");
    ExamplesTestHelper::runAppWithExpectedOutput(getExeName("Animal"), "Cat says Meow..\n");
    current_path("../../1/Animal/");
    ExamplesTestHelper::runAppWithExpectedOutput(getExeName("Animal"), "Cat says Meow..\n");
}

TEST(ExamplesTest, Example3)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example3"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/app");
    ExamplesTestHelper::runAppWithExpectedOutput(getExeName("app"),
                                                 "func1 called\nfunc2 called\nfunc3 called\nfunc4 called\n");
    current_path("../../1/app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getExeName("app"),
                                                 "func1 called\nfunc2 called\nfunc3 called\nfunc4 called\n");
}

TEST(ExamplesTest, Example4)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example4"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/app");
    ExamplesTestHelper::runAppWithExpectedOutput(getExeName("app"), "func() from file1.cpp called.\n");

    Json cacheFileJson;
    current_path("../../");
    ifstream("cache.hmake") >> cacheFileJson;
    bool file1 = cacheFileJson.at("cache-variables").get<Json>().at("FILE1").get<bool>();
    ASSERT_EQ(file1, true) << "Cache does not has the Cache-Variable or this variable is not of right value";
    cacheFileJson["cache-variables"]["FILE1"] = false;
    ofstream("cache.hmake") << cacheFileJson.dump(4);

    ASSERT_EQ(system(getSlashedExeName("configureDerived").c_str()), 0)
        << getExeName("configureDerived") + " command failed.";
    ASSERT_EQ(system(getExeName("hbuild").c_str()), 0) << getExeName("hbuild") + " command failed.";

    current_path("0/app");
    ExamplesTestHelper::runAppWithExpectedOutput(getExeName("app"), "func() from file2.cpp called.\n");
}

TEST(ExamplesTest, Example5)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example5"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getExeName("app"), "");
}

// Prebuilt libraries can have publicDependencies. But those should be named as just libraries i.e. without public.
// And these common properties, should, may be inherited. To assign a different name to these, may be, assign the
// reference to the inherited properties. Example 7 is doing the same. Example 8 depends on Example 7 and is thus
// commented out.
/*TEST(ExamplesTest, Example6)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example6"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/Animal/");
    ExamplesTestHelper::runAppWithExpectedOutput(getExeName("Animal"), "Cat says Meow..\n");
}

TEST(ExamplesTest, Example7)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example7"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject(true);
}

TEST(ExamplesTest, Example8)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example8"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/Pets/");
    ExamplesTestHelper::runAppWithExpectedOutput(getExeName("Pets"), "Dog says woof\nCat says Meow..\nGoat says baa\n");
    current_path("../../1/Pets/");
    ExamplesTestHelper::runAppWithExpectedOutput(getExeName("Pets"), "Dog says woof\nCat says Meow..\nGoat says baa\n");
}*/

#ifdef _WIN32
TEST(ExamplesTest, Example9)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example9"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("0/app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getExeName("app"), "Hello World\n");
}

TEST(ExamplesTest, Example10)
{
    current_path(path(SOURCE_DIRECTORY) / path("Examples/Example10/ball_pit"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
}
#endif
