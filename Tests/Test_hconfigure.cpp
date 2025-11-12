
#include "ExamplesTestHelper.hpp"
#include "filesystem"
#include "fstream"
#include "iostream"
#include "nlohmann/json.hpp"
#include "string"
#include "gtest/gtest.h"

using std::string, std::ofstream, std::ifstream, std::filesystem::create_directory, std::filesystem::path,
    std::filesystem::current_path, std::cout, std::format, std::filesystem::remove_all, std::ifstream, std::ofstream;

/*class HconfigureTests : public testing::Test
{
  protected:
    static void SetUpTestSuite()
    {
        current_path(path(SOURCE_DIRECTORY) / path("Tests/Stage"));
        ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
        current_path("0/app/");
        // ExamplesTestHelper::runAppWithExpectedOutput(getExeName("app"), "Hello World\n");
    }
};

TEST_F(HconfigureTests, Example1)
{
}*/