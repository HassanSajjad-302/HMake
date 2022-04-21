
#include "TestHelper.hpp"
#include "filesystem"
#include "fmt/format.h"
#include "fstream"
#include "iostream"
#include "string"
#include "gtest/gtest.h"
using std::string, std::ofstream, std::ifstream, std::filesystem::create_directory, std::filesystem::path,
    std::filesystem::current_path, std::cout, fmt::format, std::filesystem::exists, std::filesystem::remove_all;

namespace readall
{

string slurp(const std::ifstream &in)
{
    std::ostringstream str;
    str << in.rdbuf();
    return str.str();
};
} // namespace readall

TEST(CompilationTest, BasicCompilationTest)
{

    TestHelper::recreateSourceAndBuildDir();
    string hmakeFileContents = R"(
#include "Configure.hpp"

int main() {
    Cache::initializeCache();
    Project project;
    ProjectVariant variant{};

    Executable app("app", variant);
    app.sourceFiles.emplace_back("main.cpp");

    variant.executables.push_back(app);
    project.projectVariants.push_back(variant);
    project.configure();
}
)";
    string mainSrcFileContents = R"(
#include "iostream"

int main(){
    std::cout << "Hello World" << std::endl;
}
)";

    current_path("Source");
    ofstream("hmake.cpp") << hmakeFileContents;
    ofstream("main.cpp") << mainSrcFileContents;

    ASSERT_EQ(TestHelper::runHMakeProject(), "Hello World\n") << "Hell World Output Is Expected From Simple Compile";
}

TEST(CompilationTest, MultipleFilesCompilationTest)
{
    TestHelper::recreateSourceAndBuildDir();
    const int fileCount = 9;

    string hmakeFileContents = format(R"(
#include "Configure.hpp"

    int main() {{
    Cache::initializeCache();
    Project project;
    ProjectVariant variant{{}};

    Executable app("app", variant);
    app.sourceDirectories.emplace_back(Directory("."), "file[1-{}]\\.cpp|main\\.cpp"); //fileCount assigned here

    variant.executables.push_back(app);
    project.projectVariants.push_back(variant);
    project.configure();
    }}
)",
                                      fileCount);

    string mainSrcFileContents = R"(
#include "file1.hpp"
#include "iostream"
int main(){
std::cout << "0" << std::endl;
func1();
}
)";

    current_path("Source");
    ofstream("hmake.cpp") << hmakeFileContents;
    ofstream("main.cpp") << mainSrcFileContents;

    for (int i = 0; i < fileCount; ++i)
    {
        string headerFileContents = format(R"(
extern void func{}();
)",
                                           i + 1, i + 1);

        string sourceFileContents;
        if (i != fileCount - 1)
        {
            sourceFileContents = format(R"(
#include "file{}.hpp"
#include "iostream"
void func{}(){{

std::cout << {} << std::endl;
func{}();
}}
)",
                                        i + 2, i + 1, i + 1, i + 2);
        }
        else
        {
            // Last Source File
            sourceFileContents = format(R"(
#include "file{}.hpp"
#include "iostream"
void func{}(){{

std::cout << {} << std::endl;
}}
)",
                                        i + 1, i + 1, i + 1);
        }

        ofstream(format("file{}.hpp", i + 1)) << headerFileContents;
        ofstream(format("file{}.cpp", i + 1)) << sourceFileContents;
    }

    string expected;
    for (int i = 0; i < fileCount + 1; ++i)
    {
        expected += std::to_string(i) + "\n";
    }
    ASSERT_EQ(TestHelper::runHMakeProject(), expected);
}

int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
