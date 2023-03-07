#include "BuildSystemFunctions.hpp"
#include "ExamplesTestHelper.hpp"
#include "Features.hpp"
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

static string configureBuildStr = getSlashedExecutableName("configure") + " --build";

static void executeSubTest1(Test1Setup setup)
{
    // Running configure.exe --build should not update any file
    path p = current_path();
    current_path(setup.hbuildExecutionPath);
    Snapshot snapshot(p);
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    snapshot.after(p);
    ASSERT_EQ(snapshot.snapshotBalancesTest1(setup.sourceFileUpdated, setup.executableFileUpdated), true);

    snapshot.before(p);
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    snapshot.after(p);
    current_path(p);
    ASSERT_EQ(snapshot.snapshotBalancesTest1(false, false), true);
}

static void executeSubTest2(Test2Setup setup)
{
    // Running configure.exe --build should not update any file
    path p = current_path();
    current_path(setup.hbuildExecutionPath);
    Snapshot snapshot(p);
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    snapshot.after(p);
    ASSERT_EQ(snapshot.snapshotBalancesTest2(setup), true);

    snapshot.before(p);
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    snapshot.after(p);
    current_path(p);
    ASSERT_EQ(snapshot.snapshotBalancesTest2(Test2Setup{}), true);
}

static void executeSnapshotBalances(unsigned short filesCompiled, unsigned short cppTargets,
                                    unsigned short linkTargetsNoDebug, unsigned short linkTargetsDebug,
                                    const path &hbuildExecutionPath = current_path())
{
    // Running configure.exe --build should not update any file
    path p = current_path();
    current_path(hbuildExecutionPath);
    Snapshot snapshot(p);
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    snapshot.after(p);
    ASSERT_EQ(snapshot.snapshotBalances(filesCompiled, cppTargets, linkTargetsNoDebug, linkTargetsDebug), true);

    snapshot.before(p);
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    snapshot.after(p);
    current_path(p);
    ASSERT_EQ(snapshot.snapshotBalances(0, 0, 0, 0), true);
}

// Tests Hello-World and rebuild in different directories on touching file.
/*
TEST(StageTests, Test1)
{
    path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test1");
    current_path(testSourcePath);
    copyFilePath(testSourcePath / "Version/hmake0.cpp", testSourcePath / "hmake.cpp");
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getSlashedExecutableName("app"), "Hello World\n");
    current_path("../");

    executeSubTest1(Test1Setup{});

    Snapshot snapshot(current_path());

    // Touching main.cpp. Running app using configure --build.
    path mainFilePath = testSourcePath / "main.cpp";
    touchFile(mainFilePath);
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(true, true), true);

    // Touching main.cpp. But hbuild executed in app-cpp.
    touchFile(mainFilePath);
    executeSubTest1(Test1Setup{.hbuildExecutionPath = "app-cpp/", .sourceFileUpdated = true});
    executeSubTest1(Test1Setup{.hbuildExecutionPath = "app-cpp/"});

    // Now executing again in Build
    executeSubTest1(Test1Setup{.executableFileUpdated = true});

    // Touching main.cpp. But hbuild executed in app
    touchFile(mainFilePath);
    executeSubTest1(
        Test1Setup{.hbuildExecutionPath = "app/", .sourceFileUpdated = true, .executableFileUpdated = true});
    executeSubTest1(Test1Setup{.hbuildExecutionPath = "app/"});

    // Deleting app.exe
    path appExeFilePath =
        testSourcePath / "Build/app" / path(getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"));
    removeFilePath(appExeFilePath);
    executeSubTest1(Test1Setup{.executableFileUpdated = true});

    // Deleting app.exe. But hbuild executed in app-cpp first and then in app
    removeFilePath(appExeFilePath);
    executeSubTest1(Test1Setup{.hbuildExecutionPath = "app-cpp/"});
    executeSubTest1(Test1Setup{.hbuildExecutionPath = "app/", .executableFileUpdated = true});

    // Deleting app-cpp.cache
    path appCppCacheFilePath = testSourcePath / "Build/app-cpp/Cache_Build_Files/app-cpp.cache";
    removeFilePath(appCppCacheFilePath);
    executeSubTest1(Test1Setup{.sourceFileUpdated = true, .executableFileUpdated = true});

    // Deleting app-cpp.cache but executing hbuild in app
    removeFilePath(appCppCacheFilePath);
    executeSubTest1(
        Test1Setup{.hbuildExecutionPath = "app/", .sourceFileUpdated = true, .executableFileUpdated = true});

    // Deleting main.cpp.o
    path mainDotCppDotOFilePath = testSourcePath / "Build/app-cpp/Cache_Build_Files/main.cpp.o";
    removeFilePath(mainDotCppDotOFilePath);
    executeSubTest1(Test1Setup{.sourceFileUpdated = true, .executableFileUpdated = true});

    // Deleting main.cpp.o but executing in app/
    removeFilePath(mainDotCppDotOFilePath);
    executeSubTest1(
        Test1Setup{.hbuildExecutionPath = "app/", .sourceFileUpdated = true, .executableFileUpdated = true});

    // Updating compiler-flags
    copyFilePath(testSourcePath / "Version/hmake1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSubTest1(Test1Setup{.sourceFileUpdated = true, .executableFileUpdated = true});

    // Updating compiler-flags but executing in app
    copyFilePath(testSourcePath / "Version/hmake0.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSubTest1(
        Test1Setup{.hbuildExecutionPath = "app/", .sourceFileUpdated = true, .executableFileUpdated = true});

    // Updating compiler-flags but executing in app-cpp
    copyFilePath(testSourcePath / "Version/hmake1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSubTest1(Test1Setup{.hbuildExecutionPath = "app-cpp/", .sourceFileUpdated = true});

    // Executing in Build. Only app to be updated.
    executeSubTest1(Test1Setup{.executableFileUpdated = true});
}
*/

// Tests Property Transitiviy, rebuild in multiple directories on touching file, source-file inclusion and exclusion,
// header-files exclusion and inclusion, libraries exclusion and inclusion.
TEST(StageTests, Test2)
{
    path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test2");
    current_path(testSourcePath);
    copyFilePath(testSourcePath / "Version/0/main.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/0/public-lib1.hpp", testSourcePath / "lib1/public/public-lib1.hpp");

    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("Debug/app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getSlashedExecutableName("app"), "36\n");
    current_path("../../");

    executeSubTest2(Test2Setup{});

    // Touching main.cpp
    path mainFilePath = testSourcePath / "main.cpp";
    touchFile(mainFilePath);
    executeSubTest2(Test2Setup{.mainDotCpp = true});

    // Touching public-lib3.hpp
    path publicLib3DotHpp = testSourcePath / "lib3/public/public-lib3.hpp";
    touchFile(publicLib3DotHpp);
    executeSubTest2(Test2Setup{.publicLib3DotHpp = true});

    // Touching private-lib1 main.cpp
    path privateLib1DotHpp = testSourcePath / "lib1/private/private-lib1.hpp";
    touchFile(mainFilePath);
    touchFile(privateLib1DotHpp);
    executeSubTest2(Test2Setup{.mainDotCpp = true, .privateLib1DotHpp = true});

    // Touching lib4.cpp
    path lib4DotCpp = testSourcePath / "lib4/private/lib4.cpp";
    touchFile(lib4DotCpp);
    executeSubTest2(Test2Setup{.lib4DotCpp = true});

    // Touching public-lib4.hpp
    path publicLib4DotHpp = testSourcePath / "lib4/public/public-lib4.hpp";
    touchFile(publicLib4DotHpp);
    executeSubTest2(Test2Setup{.publicLib4DotHpp = true});

    // Deleting app-cpp.cache
    path lib3CppCacheFilePath = testSourcePath / "Build/Debug/lib3-cpp/Cache_Build_Files/lib3-cpp.cache";
    removeFilePath(lib3CppCacheFilePath);
    executeSubTest2(Test2Setup{.lib3DotCpp = true});

    // Deleting lib4 and lib2's app-cpp.cache
    path lib4 = testSourcePath / "Build/Debug/lib4/" /
                path(getActualNameFromTargetName(TargetType::LIBRARY_STATIC, os, "lib4"));
    path lib2CppCacheFilePath = testSourcePath / "Build/Debug/lib2-cpp/Cache_Build_Files/lib2-cpp.cache";
    removeFilePath(lib4);
    removeFilePath(lib2CppCacheFilePath);
    executeSubTest2(Test2Setup{.lib2DotCpp = true, .lib4Linked = true});

    // Touching main.cpp lib1.cpp lib1.hpp-public lib4.hpp-public
    path lib1DotCpp = testSourcePath / "lib1/private/lib1.cpp";
    path publicLib1DotHpp = testSourcePath / "lib1/public/public-lib1.hpp";
    touchFile(mainFilePath);
    touchFile(lib1DotCpp);
    touchFile(publicLib1DotHpp);
    touchFile(publicLib4DotHpp);
    executeSubTest2(Test2Setup{.mainDotCpp = true, .publicLib1DotHpp = true, .publicLib4DotHpp = true});

    // Touching public-lib4 then running hbuild in lib4-cpp, lib3-cpp, lib3, Build
    touchFile(publicLib4DotHpp);
    executeSnapshotBalances(1, 1, 0, 0, "Debug/lib4-cpp");
    executeSnapshotBalances(1, 1, 0, 0, "Debug/lib3-cpp");
    executeSnapshotBalances(0, 0, 1, 0, "Debug/lib3");
    executeSnapshotBalances(1, 1, 2, 1);

    // Touching lib2.cpp, then executing in lib4, lib3-cpp, lib3, lib1, lib1-cpp, app
    path lib2DotCpp = testSourcePath / "lib2/private/lib2.cpp";
    touchFile(lib2DotCpp);
    executeSnapshotBalances(0, 0, 0, 0, "Debug/lib4");
    executeSnapshotBalances(0, 0, 0, 0, "Debug/lib3-cpp");
    executeSnapshotBalances(0, 0, 0, 0, "Debug/lib3");
    executeSnapshotBalances(0, 0, 0, 0, "Debug/lib1");
    executeSnapshotBalances(0, 0, 0, 0, "Debug/lib1-cpp");
    executeSnapshotBalances(1, 1, 1, 1, "Debug/app");

    // Touching main.cpp lib1.hpp-public, then hbuild in app
    touchFile(mainFilePath);
    touchFile(publicLib1DotHpp);
    executeSnapshotBalances(2, 2, 1, 1, "Debug/app");

    // TODO
    // After reconfiguring, run hbuild in other directories.

    // Adding public-lib1.hpp contents to main.cpp
    // removeFilePath(testSourcePath / "Version/0/main.cpp", testSourcePath / "main.cpp");
}
