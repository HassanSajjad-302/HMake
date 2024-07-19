

#include "C_API.hpp"
#include "ExamplesTestHelper.hpp"
#include "zDLLLoader.hpp"
#include "gtest/gtest.h"
#include <filesystem>

using std::filesystem::path, std::filesystem::current_path;

TEST(CAPITEST, Test1)
{
    const path testSrcDir(path(SOURCE_DIRECTORY) / path("Tests/C_API/"));
    current_path(testSrcDir);
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    DLLLoader loader(
        ((testSrcDir / "Build/").string() + getActualNameFromTargetName(TargetType::LIBRARY_SHARED, os, "configure"))
            .c_str());

    ASSERT_NE(getCTargetContainer, nullptr) << "Dynamic library configure could not be loaded\n";

    typedef int (*Func2)(BSMode bsMode);
    const auto func2 = loader.getSymbol<Func2>("func2");

    ASSERT_NE(func2, nullptr) << "Symbol func2 could not be loaded from configure dynamic library\n";

    ASSERT_EQ(func2(BSMode::IDE), EXIT_SUCCESS) << "func2(BSMode::IDE) call failed\n";

    typedef C_TargetContainer *(*GetCTargetContainer)(BSMode bsMode);
    const auto getCTargetContainer = loader.getSymbol<GetCTargetContainer>("getCTargetContainer");

    ASSERT_NE(getCTargetContainer, nullptr)
        << "Symbol getCTargetContainer could not be loaded from configure dynamic library\n";

    const auto *cTargetContainer = getCTargetContainer(BSMode::IDE);

    ASSERT_EQ(cTargetContainer->size, 4);
    const path testBuildDir(testSrcDir / "Build/");
    for (unsigned short i = 0; i < cTargetContainer->size; ++i)
    {
        if (cTargetContainer->c_cTargets[i]->type == C_CONFIGURE_TARGET_TYPE)
        {
            const auto c_cTarget = reinterpret_cast<C_CTarget *>(cTargetContainer->c_cTargets[i]->object);
            ASSERT_EQ(equivalent(path(c_cTarget->dir), testBuildDir / "Custom/"), true);
        }
        else if (cTargetContainer->c_cTargets[i]->type == C_CPP_TARGET_TYPE)
        {
            const auto c_cppSourceTarget = reinterpret_cast<C_CppSourceTarget *>(cTargetContainer->c_cTargets[i]->object);
            ASSERT_EQ(c_cppSourceTarget->sourceFilesCount, 1);
            ASSERT_EQ(equivalent(path(c_cppSourceTarget->sourceFiles[0]), testSrcDir / "main.cpp"), true);
            ASSERT_EQ(equivalent(path(c_cppSourceTarget->parent->dir), testBuildDir / "Debug/app-cpp/"), true);
            ASSERT_NE(string(c_cppSourceTarget->compileCommand).size(), 0);
            ASSERT_NE(string(c_cppSourceTarget->compilerPath).size(), 0);
        }
        else if (cTargetContainer->c_cTargets[i]->type == C_CONFIGURATION_TARGET_TYPE)
        {
            const auto c_configuration = reinterpret_cast<C_Configuration *>(cTargetContainer->c_cTargets[i]->object);
            ASSERT_EQ(equivalent(path(c_configuration->parent->dir), testBuildDir / "Debug/"), true);
        }
        else if (cTargetContainer->c_cTargets[i]->type == C_LOA_TARGET_TYPE)
        {
            const auto c_linkOrArchiveTarget =
                reinterpret_cast<C_LinkOrArchiveTarget *>(cTargetContainer->c_cTargets[i]->object);
            ASSERT_EQ(equivalent(path(c_linkOrArchiveTarget->parent->dir), testBuildDir / "Debug/app/"), true);
        }
    }
}