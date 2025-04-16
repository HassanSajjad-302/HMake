#include "BuildSystemFunctions.hpp"
#include "ExamplesTestHelper.hpp"
#include "Features.hpp"
#include "SMFile.hpp"
#include "Snapshot.hpp"
#include "Utilities.hpp"
#include "fmt/core.h"
#include "fmt/format.h"
#include "gtest/gtest.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using std::string, std::ofstream, std::ifstream, std::filesystem::create_directory, std::filesystem::create_directories,
    std::filesystem::path, std::cout, fmt::format, std::filesystem::remove_all, std::ifstream, std::ofstream,
    std::filesystem::remove, std::filesystem::remove_all, std::filesystem::copy_file, std::error_code,
    std::filesystem::copy_options, fmt::print;

static void touchFile(const path &filePath)
{
    string command;
    if constexpr (os == OS::NT)
    {
        command = FORMAT("powershell (ls {}).LastWriteTime = Get-Date", addQuotes(filePath.string()));
    }
    else if constexpr (os == OS::LINUX)
    {
        command = FORMAT("touch {}", addQuotes(filePath.string()));
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
    if (const bool removed = remove(filePath, ec); !removed || ec)
    {
        print(stderr, "Could Not Remove the filePath {}\nError {}", filePath.string(), ec ? ec.message() : "");
        exit(EXIT_FAILURE);
    }
}

static void removeDirectory(const path &filePath)
{
    error_code ec;
    if (const bool removed = remove_all(filePath, ec); !removed || ec)
    {
        print(stderr, "Could Not Remove the filePath {}\nError {}", filePath.string(), ec ? ec.message() : "");
        exit(EXIT_FAILURE);
    }
}

static void copyFilePath(const path &sourceFilePath, const path &destinationFilePath)
{
    error_code ec;
    if (const bool copied = copy_file(sourceFilePath, destinationFilePath, copy_options::overwrite_existing, ec);
        !copied || ec)
    {
        print(stderr, "Could Not Copy the filePath {} to {} \nError {}", sourceFilePath.string(),
              destinationFilePath.string(), ec ? ec.message() : "");
        exit(EXIT_FAILURE);
    }
    if constexpr (os == OS::NT)
    {
        // TODO
        // On Windows copying does not edit the last-update-time. Not investing further atm.
        touchFile(destinationFilePath);
    }
}

#include <stacktrace>
static void executeSnapshotBalances(const Updates &updates, const path &hbuildExecutionPath = current_path())
{
    // Running configure.exe --build should not update any file
    const path p = current_path();
    current_path(hbuildExecutionPath);
    Snapshot snapshot(p);
    const int exitCode = system(hbuildBuildStr.c_str());
    /*if (exitCode)
    {
        std::cout << std::stacktrace::current() << std::endl;
    }*/
    ASSERT_EQ(exitCode, 0) << hbuildBuildStr + " command failed.";
    snapshot.after(p);
    ASSERT_EQ(snapshot.snapshotBalances(updates), true);

    snapshot.before(p);
    const int exitCode2 = system(hbuildBuildStr.c_str());
    /*if (exitCode2)
    {
        std::cout << std::stacktrace::current() << std::endl;
    }*/
    ASSERT_EQ(exitCode2, 0) << hbuildBuildStr + " command failed.";
    snapshot.after(p);
    current_path(p);
    ASSERT_EQ(snapshot.snapshotBalances(Updates{}), true);
}

static void executeTwoSnapshotBalances(const Updates &updates1, const Updates &updates2,
                                       const path &hbuildExecutionPath = current_path())
{
    // Running configure.exe --build should not update any file
    const path p = current_path();
    current_path(hbuildExecutionPath);
    Snapshot snapshot(p);
    const int exitCode = system(hbuildBuildStr.c_str());
    ASSERT_EQ(exitCode, 0) << hbuildBuildStr + " command failed.";
    snapshot.after(p);
    ASSERT_EQ(snapshot.snapshotBalances(updates1), true);

    snapshot.before(p);
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    snapshot.after(p);
    current_path(p);
    ASSERT_EQ(snapshot.snapshotBalances(updates2), true);
}

static void executeErroneousSnapshotBalances(const Updates &updates, const path &hbuildExecutionPath = current_path())
{
    // Running configure.exe --build should not update any file
    const path p = current_path();
    current_path(hbuildExecutionPath);
    Snapshot snapshot(p);
    system(hbuildBuildStr.c_str());
    snapshot.after(p);
    ASSERT_EQ(snapshot.snapshotBalances(updates), true);
    current_path(p);
}

// Tests Hello-World and rebuild in different dirs on touching file.
TEST(StageTests, Test1)
{
    const path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test1");
    current_path(testSourcePath);
    copyFilePath(testSourcePath / "Version/hmake0.cpp", testSourcePath / "hmake.cpp");
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("Release/app/");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/app", "Hello World\n");
    current_path("../../");

    executeSnapshotBalances(Updates{});

    // Touching main.cpp.
    const path mainFilePath = testSourcePath / "main.cpp";
    touchFile(mainFilePath);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1});

    // Touching main.cpp. But hbuild executed in app-cpp.
    touchFile(mainFilePath);
    // Test currently failing because app-cpp is already added in nodes.json because of buildCacheFilesDirPath.
    executeSnapshotBalances(Updates{.sourceFiles = 1}, "Release/app-cpp/");

    // Now executing again in Build
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1});

    // Touching main.cpp. But hbuild executed in app
    touchFile(mainFilePath);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1}, "Release/app/");

    // Deleting app.exe
    const path appExeFilePath =
        testSourcePath / "Build/Release/app" / path(getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"));
    removeFilePath(appExeFilePath);
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1});

    // Deleting app.exe. But hbuild executed in app-cpp first and then in app
    removeFilePath(appExeFilePath);
    executeSnapshotBalances(Updates{}, "Release/app-cpp/");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1}, "Release/app/");

    // Deleting app-cpp dir
    const path appCppDirectory = testSourcePath / "Build/Release/app-cpp/";
    removeDirectory(appCppDirectory);
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1});

    // Deleting app-cpp dir but executing hbuild in app
    removeDirectory(appCppDirectory);
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1}, "Release/app/");

    // Deleting main.cpp.o
    const path mainDotCppDotOFilePath = testSourcePath / "Build/Release/app-cpp/main.cpp.o";
    removeFilePath(mainDotCppDotOFilePath);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1});

    // Deleting main.cpp.o but executing in app/
    removeFilePath(mainDotCppDotOFilePath);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1}, "Release/app/");

    // Updating compiler-flags
    copyFilePath(testSourcePath / "Version/hmake1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1});

    // Updating compiler-flags but executing in app
    copyFilePath(testSourcePath / "Version/hmake0.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1});

    // Updating compiler-flags but executing in app-cpp
    copyFilePath(testSourcePath / "Version/hmake1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.sourceFiles = 1}, "Release/app-cpp/");

    // Executing in Build. Only app to be updated.
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1});
}

static void setupTest2Default()
{
    const path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test2");
    copyFilePath(testSourcePath / "Version/0/hmake.cpp", testSourcePath / "hmake.cpp");
    copyFilePath(testSourcePath / "Version/0/main.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/0/public-lib1.hpp", testSourcePath / "lib1/public/public-lib1.hpp");
    copyFilePath(testSourcePath / "Version/0/lib1.cpp", testSourcePath / "lib1/private/lib1.cpp");
    copyFilePath(testSourcePath / "Version/0/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");

    const path extraIncludeFilePath = testSourcePath / "lib1/public/extra-include.hpp";
    if (exists(extraIncludeFilePath))
    {
        removeFilePath(extraIncludeFilePath);
    }
    const path tempLib4Path = testSourcePath / "lib4/private/temp.cpp";
    if (exists(tempLib4Path))
    {
        removeFilePath(tempLib4Path);
    }
}

// Tests Property Transitiviy, rebuild in multiple dirs on touching file, source-file inclusion and exclusion,
// header-files exclusion and inclusion, libraries exclusion and inclusion, caching in-case of error in
// file-compilation.

// Haven't noticed recently.
// Test2 clean build subtest fails sometimes on Linux. While debugging, target HMakeHelper returned 66 EREMOTE error
// even when the program breakpoint on the line exit(EXIT_SUCCESS).
TEST(StageTests, Test2)
{
    path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test2");
    current_path(testSourcePath);
    setupTest2Default();

    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("Debug/app/");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/app", "36\n");
    current_path("../../");

    executeSnapshotBalances(Updates{});

    // Touching main.cpp
    path mainFilePath = testSourcePath / "main.cpp";
    touchFile(mainFilePath);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsDebug = 1});

    // Touching public-lib3.hpp
    path publicLib3DotHpp = testSourcePath / "lib3/public/public-lib3.hpp";
    touchFile(publicLib3DotHpp);
    executeSnapshotBalances(Updates{.sourceFiles = 2, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Touching private-lib1 and main.cpp
    path privateLib1DotHpp = testSourcePath / "lib1/private/private-lib1.hpp";
    touchFile(mainFilePath);
    touchFile(privateLib1DotHpp);
    executeSnapshotBalances(Updates{.sourceFiles = 2, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching lib4.cpp
    path lib4DotCpp = testSourcePath / "lib4/private/lib4.cpp";
    touchFile(lib4DotCpp);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching public-lib4.hpp
    path publicLib4DotHpp = testSourcePath / "lib4/public/public-lib4.hpp";
    touchFile(publicLib4DotHpp);
    executeSnapshotBalances(Updates{.sourceFiles = 3, .linkTargetsNoDebug = 3, .linkTargetsDebug = 1});

    // Deleting lib3-cpp dir
    path lib3CppDirectory = testSourcePath / "Build/Debug/lib3-cpp/";
    removeDirectory(lib3CppDirectory);
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Deleting lib4 and lib2-cpp dir
    path lib4 = testSourcePath / "Build/Debug/lib4/" /
                path(getActualNameFromTargetName(TargetType::LIBRARY_STATIC, os, "lib4"));
    path lib2CppDirectory = testSourcePath / "Build/Debug/lib2-cpp/";
    removeFilePath(lib4);
    removeDirectory(lib2CppDirectory);
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Touching main.cpp lib1.cpp lib1.hpp-public lib4.hpp-public
    path lib1DotCpp = testSourcePath / "lib1/private/lib1.cpp";
    path publicLib1DotHpp = testSourcePath / "lib1/public/public-lib1.hpp";
    touchFile(mainFilePath);
    touchFile(lib1DotCpp);
    touchFile(publicLib1DotHpp);
    touchFile(publicLib4DotHpp);
    executeSnapshotBalances(Updates{.sourceFiles = 5, .linkTargetsNoDebug = 4, .linkTargetsDebug = 1});

    // Touching public-lib4 then running hbuild in lib4-cpp, lib3-cpp, lib3, Build
    touchFile(publicLib4DotHpp);
    executeSnapshotBalances(Updates{.sourceFiles = 1}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1}, "Debug/lib3");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Touching lib2.cpp, then executing in lib4, lib3-cpp, lib3, lib1, lib1-cpp, app
    path lib2DotCpp = testSourcePath / "lib2/private/lib2.cpp";
    touchFile(lib2DotCpp);
    executeSnapshotBalances(Updates{}, "Debug/lib4");
    executeSnapshotBalances(Updates{}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{}, "Debug/lib3");
    executeSnapshotBalances(Updates{}, "Debug/lib1");
    executeSnapshotBalances(Updates{}, "Debug/lib1-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1}, "Debug/app");

    // Touching main.cpp lib1.hpp-public, then hbuild in app
    touchFile(mainFilePath);
    touchFile(publicLib1DotHpp);
    executeSnapshotBalances(Updates{.sourceFiles = 2, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1}, "Debug/app");

    // Adding public-lib1.hpp contents to main.cpp and lib1.cpp and removing it from dir
    copyFilePath(testSourcePath / "Version/1/main.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/1/lib1.cpp", testSourcePath / "lib1/private/lib1.cpp");
    removeFilePath(testSourcePath / "lib1/public/public-lib1.hpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1}, "Debug/lib1-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1}, "Debug/app");
    executeSnapshotBalances(Updates{});

    // Replacing public-lib1.hpp with two header-files and restoring lib1.cpp and main.cpp
    copyFilePath(testSourcePath / "Version/0/main.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/0/lib1.cpp", testSourcePath / "lib1/private/lib1.cpp");
    copyFilePath(testSourcePath / "Version/2/public-lib1.hpp", testSourcePath / "lib1/public/public-lib1.hpp");
    copyFilePath(testSourcePath / "Version/2/extra-include.hpp", testSourcePath / "lib1/public/extra-include.hpp");
    executeSnapshotBalances(Updates{}, "Debug/lib2-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .nodesFile = true}, "Debug/lib1-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Resorting to the default-version for the project
    setupTest2Default();
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";

    // Removing all libraries, making main simple and reconfiguring the project.
    copyFilePath(testSourcePath / "Version/3/main.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/3/hmake.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";

    executeSnapshotBalances(Updates{}, "Debug/lib2-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1}, "Debug/app-cpp");
    executeSnapshotBalances(Updates{.linkTargetsDebug = 1}, "Debug/app");

    // Resorting to the old-main and reconfiguring the project.
    copyFilePath(testSourcePath / "Version/0/main.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/0/hmake.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";

    executeSnapshotBalances(Updates{}, "Debug/lib2-cpp");
    executeSnapshotBalances(Updates{}, "Debug/lib4");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsDebug = 1});
    // Moving lib4.cpp code to temp.cpp in lib4/
    removeFilePath(testSourcePath / "lib4/private/lib4.cpp");
    copyFilePath(testSourcePath / "Version/4/temp.cpp", testSourcePath / "lib4/private/temp.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{}, "Debug/lib2-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .nodesFile = true}, "Debug/lib4");
    executeSnapshotBalances(Updates{.linkTargetsDebug = 1});

    // Copying an erroneous lib4.cpp to lib4/private. Also touching temp.cpp and removing liblib3.lib
    copyFilePath(testSourcePath / "Version/5/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    touchFile(testSourcePath / "lib4/private/temp.cpp");
    removeFilePath(testSourcePath / "Build/Debug/lib3/" /
                   getActualNameFromTargetName(TargetType::LIBRARY_STATIC, os, "lib3"));
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeErroneousSnapshotBalances(Updates{.errorFiles = 1, .sourceFiles = 1, .linkTargetsNoDebug = 1});
    executeErroneousSnapshotBalances(Updates{.errorFiles = 1});
    executeSnapshotBalances(Updates{}, "Debug/lib3");

    // Erroneous lib4.cpp replaced by an empty lib4.cpp
    copyFilePath(testSourcePath / "Version/6/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    executeSnapshotBalances(Updates{}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Copying Erroneous lib4.cpp to lib4/private and changing the hmake.cpp and reconfiguring the project.
    copyFilePath(testSourcePath / "Version/5/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    copyFilePath(testSourcePath / "Version/7/hmake.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";

    create_directories("Release/lib3/");
    create_directories("Release/lib4/");
    executeErroneousSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .nodesFile = true},
                                     "Release/lib3/");
    executeErroneousSnapshotBalances(Updates{}, "Release/lib3/");
    executeErroneousSnapshotBalances(Updates{.errorFiles = 1, .sourceFiles = 1, .nodesFile = true}, "Release/lib4/");
    executeErroneousSnapshotBalances(
        Updates{.errorFiles = 1, .sourceFiles = 3, .linkTargetsNoDebug = 2, .nodesFile = true});
    executeErroneousSnapshotBalances(Updates{.errorFiles = 1});

    // Copying Empty lib4.cpp
    copyFilePath(testSourcePath / "Version/6/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    executeSnapshotBalances(Updates{}, "Release/lib3/");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 2});

    // Restoring lib4.cpp. This hmake.cpp will make the selection between lib4.cpp and temp.cpp based on the cache
    // variable use-lib4.cpp value
    copyFilePath(testSourcePath / "Version/8/hmake.cpp", testSourcePath / "hmake.cpp");
    copyFilePath(testSourcePath / "Version/0/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{}, "Debug/lib2-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1}, "Debug/lib4");
    executeSnapshotBalances(Updates{.linkTargetsDebug = 1});

    path cacheFile = testSourcePath / "Build/cache.json";
    Json cacheJson;
    ifstream(cacheFile) >> cacheJson;
    // Changing the cache variable to false should cause only the relinking, but not the recompilation of temp.cpp.
    cacheJson["cache-variables"]["use-lib4.cpp"] = false;
    ofstream(cacheFile) << cacheJson.dump(4);
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{}, "Debug/lib2-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1}, "Debug/app");
    executeSnapshotBalances(Updates{});
}

static void setupTest3Default()
{
    const path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test3");
    copyFilePath(testSourcePath / "Version/0/hmake.cpp", testSourcePath / "hmake.cpp");
    copyFilePath(testSourcePath / "Version/0/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    copyFilePath(testSourcePath / "Version/0/lib3.cpp", testSourcePath / "lib3/private/lib3.cpp");
    copyFilePath(testSourcePath / "Version/0/lib2.cpp", testSourcePath / "lib2/private/lib2.cpp");

    const path extraIncludeFilePath = testSourcePath / "lib1/public/extra-include.hpp";
    if (exists(extraIncludeFilePath))
    {
        removeFilePath(extraIncludeFilePath);
    }
    const path tempLib4Path = testSourcePath / "lib4/private/temp.cpp";
    if (exists(tempLib4Path))
    {
        removeFilePath(tempLib4Path);
    }
}

// Tests for source-files replacement with modules-files or otherwise, modules-files in-conjunction with source-files in
// same target, different cppSourceTargets with same module-scope, caching, multiple header-units in same target, error
// in-compilation in one file, reconfiguration, header-files exclusion-inclusion, module/source-files/header-units
// exclusion/inclusion.

#ifdef _WIN32

TEST(StageTests, Test3)
{
    const path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test3");
    current_path(testSourcePath);
    setupTest3Default();

    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("Debug/app/");
    ExamplesTestHelper::runAppWithExpectedOutput("app", "36\n");
    current_path("../../");

    executeSnapshotBalances(Updates{});

    // Making lib3.cpp a module-file instead of a source-file with public/lib3 as hu-include. i.e. one header-unit and
    // one module-file to be recompiled.

    copyFilePath(testSourcePath / "Version/1/hmake.cpp", testSourcePath / "hmake.cpp");
    copyFilePath(testSourcePath / "Version/1/lib3.cpp", testSourcePath / "lib3/private/lib3.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.smruleFiles = 2, .nodesFile = true}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching public-lib3.hpp
    const path publicLib3DotHpp = testSourcePath / "lib3/public/public-lib3.hpp";
    touchFile(publicLib3DotHpp);
    executeSnapshotBalances(Updates{.smruleFiles = 2}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1}, "Debug/lib2");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching public-lib4.hpp
    const path publicLib4DotHpp = testSourcePath / "lib4/public/public-lib4.hpp";
    touchFile(publicLib4DotHpp);
    executeSnapshotBalances(Updates{.smruleFiles = 2}, "Debug/lib1-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1}, "Debug/lib2-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1}, "Debug/lib2");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Making lib4.cpp and lib2.cpp both module-files.
    // 4 smrules files lib4.cpp lib3.cpp public-lib4.hpp privte-lib4.hpp will be updated
    copyFilePath(testSourcePath / "Version/3/hmake.cpp", testSourcePath / "hmake.cpp");
    copyFilePath(testSourcePath / "Version/3/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    copyFilePath(testSourcePath / "Version/3/lib2.cpp", testSourcePath / "lib2/private/lib2.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.smruleFiles = 4, .nodesFile = true}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 2}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Removing both header-units from lib4.cpp
    copyFilePath(testSourcePath / "Version/4/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    executeSnapshotBalances(Updates{.smruleFiles = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Adding both header-units back in lib4.cpp
    copyFilePath(testSourcePath / "Version/3/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    executeSnapshotBalances(Updates{.smruleFiles = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    //  Touching public-lib4.hpp.
    //  lib3.cpp has a header-unit dep on public-lib3.hpp which has a header-dep on public-lib4.hpp, i.e.
    //  public-lib3.hpp will be recompiled and its dependent lib3.cpp will also be recompiled but public-lib4.hpp is
    //  itself a header-unit in lib4.cpp, so its smrule will also be generated.
    touchFile(testSourcePath / "lib4/public/public-lib4.hpp");

    // 5 smruleFiles are generated. lib2.cpp, lib3.cpp, lib4.cpp, public-lib3.hpp, public-lib4.hpp.
    executeSnapshotBalances(Updates{.smruleFiles = 5}, "Debug/lib1-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{}, "Debug/lib1-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 1, .linkTargetsNoDebug = 1}, "Debug/lib2");
    executeSnapshotBalances(Updates{.sourceFiles = 1}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Changing hmake.cpp to have option to switch lib4.cpp from module to source
    copyFilePath(testSourcePath / "Version/4/hmake.cpp", testSourcePath / "hmake.cpp");
    copyFilePath(testSourcePath / "Version/4/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.smruleFiles = 4,
                                    .sourceFiles = 2,
                                    .moduleFiles = 2,

                                    .linkTargetsNoDebug = 2,
                                    .linkTargetsDebug = 1});
    executeSnapshotBalances(Updates{});
    const path cacheFile = testSourcePath / "Build/cache.json";
    Json cacheJson;
    ifstream(cacheFile) >> cacheJson;
    // Moving from module to source, lib4.cpp will be recompiled because lib4.cpp.o was not compiled with latest
    // changes. Other file is lib2.cpp.o which is being recompiled because public-lib4.hpp is being recompiled because
    // of change of compile-command due to removal of compile-definition.
    cacheJson["cache-variables"]["use-module"] = false;
    ofstream(cacheFile) << cacheJson.dump(4);
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.smruleFiles = 2,
                                    .sourceFiles = 2,
                                    .moduleFiles = 1,

                                    .linkTargetsNoDebug = 2,
                                    .linkTargetsDebug = 1});

    cacheJson["cache-variables"]["use-module"] = true;
    ofstream(cacheFile) << cacheJson.dump(4);
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";

    // This test passes if I change less than to greater than in IndexInTopologicalSortComparatorRoundZero::operator()
    // which inverses the order of libraries. This, I guess, is a compiler or linker bug.
    // 3 smrules file include lib4.cpp, public-lib4.hpp and lib2.cpp. private-lib4.hpp is saved with same old
    // compile-command. But the public-lib4.hpp is imported in lib2.cpp, so it was recompiled when USE_MODULE definition
    // was removed.
    executeSnapshotBalances(Updates{.smruleFiles = 3}, "Debug/lib3");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 1, .linkTargetsNoDebug = 1}, "Debug/lib2");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Moving back to source from module. lib4.cpp.o should not be rebuilt because lib4.cpp with the same
    // compile-command is already in the cache but relinking should happen because previously it were source-file
    // object-files, now it is module-file object-files.
    cacheJson["cache-variables"]["use-module"] = false;
    ofstream(cacheFile) << cacheJson.dump(4);
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.smruleFiles = 2,
                                    .sourceFiles = 1,
                                    .moduleFiles = 1,

                                    .linkTargetsNoDebug = 2,
                                    .linkTargetsDebug = 1});
}

TEST(StageTests, Test4)
{
    const path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test4");
    current_path(testSourcePath);
    copyFilePath(testSourcePath / "Version/0/hmake.cpp", testSourcePath / "hmake.cpp");

    if (exists(path("Build")))
    {
        remove_all(path("Build"));
    }
    create_directory("Build");
    current_path("Build");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << "First " + hhelperStr + " command failed.";
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << "Second " + hhelperStr + " command failed.";

    executeSnapshotBalances(Updates{.smruleFiles = 2, .nodesFile = true}, "hu/aevain");

    copyFilePath(testSourcePath / "Version/1/hmake.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << "Second " + hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.smruleFiles = 2, .nodesFile = true}, "hu/aevain");
}

// A test for bighu. feature in development.
TEST(StageTests, Test5)
{
}

static void setupTest6Default()
{
    const path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test6");
    copyFilePath(testSourcePath / "Version/0/hmake.cpp", testSourcePath / "hmake.cpp");
}

// TODO
// Will be completed after configuration having vector of different cpptarget properties API is completed.
/*
TEST(StageTests, Test6)
{
    const path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test4");
    current_path(testSourcePath);
    setupTest4Default();

    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("app/");
    ExamplesTestHelper::runAppWithExpectedOutput(
        "app",
        "Cat.cpp compiled with /std:c++latest with dependency on std.cpp compiled with /std:c++latest as well\n");
    current_path("../");

    executeSnapshotBalances(Updates{});

    copyFilePath(testSourcePath / "Version/1/hmake.cpp", testSourcePath / "hmake.cpp");

    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.smruleFiles = 1}, "cat-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1}, "app");

    // TODO
    // Expand test further and maybe merge with the above test.
    // Add test where scanning fails. Both for modules and header-unit.
}
*/

#endif

// TODO
// Few features like PLOAT::outputName and PLOAT::name aren't tested. atm.
// Testing could be further expanded as-well.