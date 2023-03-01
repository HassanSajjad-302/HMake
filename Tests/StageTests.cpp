#include "BuildSystemFunctions.hpp"
#include "ExamplesTestHelper.hpp"
#include "SMFile.hpp"
#include "Snapshot.hpp"
#include "Utilities.hpp"
#include "fmt/core.h"
#include "fmt/format.h"
#include "nlohmann/json.hpp"
#include "gtest/gtest.h"
#include <filesystem>
#include <iostream>
#include <string>

using std::string, std::ofstream, std::ifstream, std::filesystem::create_directory, std::filesystem::path,
    std::filesystem::current_path, std::cout, fmt::format, std::filesystem::remove_all, std::ifstream, std::ofstream,
    std::filesystem::remove, std::filesystem::copy_file, std::error_code, std::filesystem::copy_options;

using Json = nlohmann::ordered_json;

static void touchFile(const path &filePath)
{
    string command;
    if constexpr (os == OS::NT)
    {
        command = fmt::format("powershell (ls {}).LastWriteTime = Get-Date", addQuotes(filePath.string()));
    }
    else if constexpr (os == OS::LINUX)
    {
        command = fmt::format("touch {}", addQuotes(filePath.string()));
    }
    if (system(command.c_str()) == EXIT_FAILURE)
    {
        print(stderr, "Could not touch file {}\n", filePath.string());
        exit(EXIT_FAILURE);
    }
}

static void removeFilePath(const path &filePath)
{
    error_code ec;
    if (bool removed = remove(filePath, ec); !removed || ec)
    {
        print(stderr, "Could Not Remove the filePath {}\nError {}", filePath.generic_string(), ec ? ec.message() : "");
        exit(EXIT_FAILURE);
    }
}

static void copyFilePath(const path &sourceFilePath, const path &destinationFilePath)
{
    error_code ec;
    if (bool copied = copy_file(sourceFilePath, destinationFilePath, copy_options::overwrite_existing, ec);
        !copied || ec)
    {
        print(stderr, "Could Not Copy the filePath {} to {} \nError {}", sourceFilePath.generic_string(),
              destinationFilePath.generic_string(), ec ? ec.message() : "");
        exit(EXIT_FAILURE);
    }
}

static string configureBuildStr = getSlashedExeName("configure") + " --build";

TEST(StageBasicTests, Test1_Compile)
{
    path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test1");
    current_path(testSourcePath);
    copyFilePath(testSourcePath / "Version/hmake0.cpp", testSourcePath / "hmake.cpp");
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getSlashedExeName("app"), "Hello World\n");
    current_path("../");

    {
        // Just running configure.exe --build should not update any file
        Snapshot before(current_path());
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 0, 0, 0), true);
    }
    {
        // Touching main.cpp should update 10 files. (4*2 for app.exe an main.cpp.o (response, error, output) + (2
        // target cache)
        Snapshot before(current_path());
        path mainFilePath = testSourcePath / "main.cpp";
        touchFile(mainFilePath);
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 1, 1, 2), true);
    }
    {
        // Just running configure.exe --build should not update any file
        Snapshot before(current_path());
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 0, 0, 0), true);
    }
    {
        // Deleting app.exe should result in 5 files updated (4*1 for app.exe and 1 target-cache file
        Snapshot before(current_path());
        path appExeFilePath = testSourcePath / "Build/app" / path(getExeName("app"));
        removeFilePath(appExeFilePath);
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 0, 1, 1), true);
    }
    {
        // Just running configure.exe --build should not update any file
        Snapshot before(current_path());
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 0, 0, 0), true);
    }
    {
        // Deleting app-cpp.cache should result in 10 files updated.
        Snapshot before(current_path());
        path appCppCacheFilePath = testSourcePath / "Build/app/app-cpp/Cache_Build_Files/app-cpp.cache";
        removeFilePath(appCppCacheFilePath);
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 1, 1, 2), true);
    }
    {
        // Just running configure.exe --build should not update any file
        Snapshot before(current_path());
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 0, 0, 0), true);
    }
    {
        // Deleting main.cpp.o should result in 10 files updated
        Snapshot before(current_path());
        path mainDotCppDotOFilePath = testSourcePath / "Build/app/app-cpp/Cache_Build_Files/main.cpp.o";
        removeFilePath(mainDotCppDotOFilePath);
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 1, 1, 2), true);
    }
    {
        // Just running configure.exe --build should not update any file
        Snapshot before(current_path());
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 0, 0, 0), true);
    }
    {
        // Updating compiler-flags should result in 10 files updated
        copyFilePath(testSourcePath / "Version/hmake1.cpp", testSourcePath / "hmake.cpp");
        ASSERT_EQ(system(getExeName("hhelper").c_str()), 0) << getExeName("hhelper") + " command failed.";
        Snapshot before(current_path());
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 1, 1, 2), true);
    }
    {
        // Just running configure.exe --build should not update any file
        Snapshot before(current_path());
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 0, 0, 0), true);
    }
}

/*
TEST(StageBasicTests, Test1_Touch_Main)
{
    path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test1");
    current_path(testSourcePath);
    copyFilePath(testSourcePath / "Version/hmake0.cpp", testSourcePath / "hmake.cpp");
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getSlashedExeName("app"), "Hello World\n");
    current_path("../");

    {
        // Just running configure.exe --build should not update any file
        Snapshot before(current_path());
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 0, 0), true);
    }
    {
        // Touching main.cpp should update 10 files. (4*2 for app.exe an main.cpp.o (response, error, output) + (2
        // target cache)
        Snapshot before(current_path());
        path mainFilePath = testSourcePath / "main.cpp";
        touchFile(mainFilePath);
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 2, 2), true);
    }
    {
        // Just running configure.exe --build should not update any file
        Snapshot before(current_path());
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 0, 0), true);
    }
    {
        // Deleting app.exe should result in 5 files updated (4*1 for app.exe and 1 target-cache file
        Snapshot before(current_path());
        path appExeFilePath = testSourcePath / "Build/app" / path(getSlashedExeName("app"));
        removeFilePath(appExeFilePath);
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 1, 1), true);
    }
    {
        // Just running configure.exe --build should not update any file
        Snapshot before(current_path());
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 0, 0), true);
    }
    {
        // Deleting app-cpp.cache should result in 10 files updated.
        Snapshot before(current_path());
        path appCppCacheFilePath = testSourcePath / "Build/app/app-cpp/Cache_Build_Files/app-cpp.cache";
        removeFilePath(appCppCacheFilePath);
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 2, 2), true);
    }
    {
        // Just running configure.exe --build should not update any file
        Snapshot before(current_path());
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 0, 0), true);
    }
    {
        // Deleting main.cpp.o should result in 10 files updated
        Snapshot before(current_path());
        path mainDotCppDotOFilePath = testSourcePath / "Build/app/app-cpp/Cache_Build_Files/main.cpp.o";
        removeFilePath(mainDotCppDotOFilePath);
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 2, 2), true);
    }
    {
        // Just running configure.exe --build should not update any file
        Snapshot before(current_path());
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 0, 0), true);
    }
    {
        // Updating compiler-flags should result in 10 files updated
        copyFilePath(testSourcePath / "Version/hmake1.cpp", testSourcePath / "hmake.cpp");
        ASSERT_EQ(system(getExeName("hhelper").c_str()), 0) << getExeName("hhelper") + " command failed.";
        Snapshot before(current_path());
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 2, 2), true);
    }
    {
        // Just running configure.exe --build should not update any file
        Snapshot before(current_path());
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        Snapshot after(current_path());
        ASSERT_EQ(Snapshot::snapshotBalances(before, after, 0, 0), true);
    }
}
*/

/*
TEST(StageBasicTests, Test1Compile)
{
    current_path(path(SOURCE_DIRECTORY) / path("Tests/Stage/Test1"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getSlashedExeName("app"), "Hello World\n");
}

TEST(StageBasicTests, Test1Compile)
{
    current_path(path(SOURCE_DIRECTORY) / path("Tests/Stage/Test1"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getSlashedExeName("app"), "Hello World\n");
}

TEST(StageBasicTests, Test1Compile)
{
    current_path(path(SOURCE_DIRECTORY) / path("Tests/Stage/Test1"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getSlashedExeName("app"), "Hello World\n");
}

TEST(StageBasicTests, Test1Compile)
{
    current_path(path(SOURCE_DIRECTORY) / path("Tests/Stage/Test1"));
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getSlashedExeName("app"), "Hello World\n");
}*/
