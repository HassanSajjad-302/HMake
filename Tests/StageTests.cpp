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
static string hbuildBuildStr = getSlashedExeName("hbuild");

static void noFileUpdated()
{
    // Running configure.exe --build should not update any file
    Snapshot snapshot(current_path());
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(false, false), true);
}

TEST(StageBasicTests, Test1_Compile)
{
    path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test1");
    current_path(testSourcePath);
    copyFilePath(testSourcePath / "Version/hmake0.cpp", testSourcePath / "hmake.cpp");
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getSlashedExeName("app"), "Hello World\n");
    current_path("../");

    noFileUpdated();

    Snapshot snapshot(current_path());

    // Touching main.cpp
    path mainFilePath = testSourcePath / "main.cpp";
    touchFile(mainFilePath);
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(true, true), true);

    noFileUpdated();

    // Touching main.cpp. But hbuild executed in app-cpp.
    touchFile(mainFilePath);
    current_path("app-cpp/");
    snapshot.before(current_path());
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(true, false), true);

    noFileUpdated();

    // Now executing again in build
    current_path("../");
    snapshot.before(current_path());
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(false, true), true);

    noFileUpdated();

    // Touching main.cpp. But hbuild executed in app
    touchFile(mainFilePath);
    snapshot.before(current_path());
    current_path("app/");
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    current_path("../");
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(true, true), true);

    current_path("app/");
    noFileUpdated();

    current_path("../");

    noFileUpdated();

    // Deleting app.exe
    snapshot.before(current_path());
    path appExeFilePath = testSourcePath / "Build/app" / path(getExeName("app"));
    removeFilePath(appExeFilePath);
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(false, true), true);

    noFileUpdated();

    // Deleting app.exe. But hbuild executed in app-cpp first and then in app
    snapshot.before(current_path());
    removeFilePath(appExeFilePath);
    current_path("app-cpp/");
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    current_path("../");
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(false, false), true);
    snapshot.before(current_path());
    current_path("app/");
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    current_path("../");
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(false, true), true);

    noFileUpdated();

    // Deleting app-cpp.cache
    snapshot.before(current_path());
    path appCppCacheFilePath = testSourcePath / "Build/app-cpp/Cache_Build_Files/app-cpp.cache";
    removeFilePath(appCppCacheFilePath);
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(true, true), true);

    noFileUpdated();

    // Deleting app-cpp.cache but executing hbuild in app
    removeFilePath(appCppCacheFilePath);
    snapshot.before(current_path());
    current_path("app/");
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    current_path("../");
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(true, true), true);

    current_path("app/");
    noFileUpdated();
    current_path("../");

    current_path("app-cpp/");
    noFileUpdated();
    current_path("../");

    // Deleting main.cpp.o
    snapshot.before(current_path());
    path mainDotCppDotOFilePath = testSourcePath / "Build/app-cpp/Cache_Build_Files/main.cpp.o";
    removeFilePath(mainDotCppDotOFilePath);
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(true, true), true);

    noFileUpdated();

    // Deleting main.cpp.o but executing in app/
    snapshot.before(current_path());
    removeFilePath(mainDotCppDotOFilePath);
    current_path("app/");
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    current_path("../");
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(true, true), true);

    noFileUpdated();

    // Updating compiler-flags
    copyFilePath(testSourcePath / "Version/hmake1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(getExeName("hhelper").c_str()), 0) << getExeName("hhelper") + " command failed.";
    snapshot.before(current_path());
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(true, true), true);

    // Updating compiler-flags but executing in app
    copyFilePath(testSourcePath / "Version/hmake0.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(getExeName("hhelper").c_str()), 0) << getExeName("hhelper") + " command failed.";
    snapshot.before(current_path());
    current_path("app/");
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    current_path("../");
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(true, true), true);

    current_path("app/");
    noFileUpdated();
    current_path("../");
    noFileUpdated();

    // Updating compiler-flags but executing in app-cpp
    copyFilePath(testSourcePath / "Version/hmake1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(getExeName("hhelper").c_str()), 0) << getExeName("hhelper") + " command failed.";
    snapshot.before(current_path());
    current_path("app-cpp");
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    current_path("../");
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(true, false), true);

    // Executing in Build. Only app to be updated.
    snapshot.before(current_path());
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(false, true), true);

    noFileUpdated();
}