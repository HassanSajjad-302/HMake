
#include "TestPaths.hpp"
#include "filesystem"
#include "fstream"
#include "string"
#include "gtest/gtest.h"
using std::string, std::ofstream, std::filesystem::create_directory, std::filesystem::path;

TEST(CompilationTest, BasicCompilationTest)
{
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
    ASSERT_EQ(TestPaths::compileAndRunHMakeProject(hmakeFileContents, mainSrcFileContents), "Hello World\n")
        << "Hell World Output Is Expected From Simple Compile";
}
int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
