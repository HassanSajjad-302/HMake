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
#include <fstream>
#include <iostream>
#include <string>

using std::string, std::ofstream, std::ifstream, std::filesystem::create_directory, std::filesystem::path,
    std::filesystem::current_path, std::cout, fmt::format, std::filesystem::remove_all, std::ifstream, std::ofstream,
    std::filesystem::remove, std::filesystem::copy_file, std::error_code, std::filesystem::copy_options, fmt::print;

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
    if constexpr (os == OS::NT)
    {
        // TODO
        // On Windows copying does not edit the last-update-time. Not investing further atm.
        touchFile(destinationFilePath);
    }
}

static void executeSnapshotBalances(const Updates &updates, const path &hbuildExecutionPath = current_path())
{
    // Running configure.exe --build should not update any file
    path p = current_path();
    current_path(hbuildExecutionPath);
    Snapshot snapshot(p);
    int exitCode = system(hbuildBuildStr.c_str());
    ASSERT_EQ(exitCode, 0) << hbuildBuildStr + " command failed.";
    snapshot.after(p);
    ASSERT_EQ(snapshot.snapshotBalances(updates), true);

    snapshot.before(p);
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    snapshot.after(p);
    current_path(p);
    ASSERT_EQ(snapshot.snapshotBalances(Updates{}), true);
}

static void executeErroneousSnapshotBalances(const Updates &updates, const path &hbuildExecutionPath = current_path())
{
    // Running configure.exe --build should not update any file
    path p = current_path();
    current_path(hbuildExecutionPath);
    Snapshot snapshot(p);
    system(hbuildBuildStr.c_str());
    snapshot.after(p);
    ASSERT_EQ(snapshot.snapshotBalances(updates), true);
    current_path(p);
}

// Tests Hello-World and rebuild in different directories on touching file.
TEST(StageTests, Test1)
{
    path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test1");
    current_path(testSourcePath);
    copyFilePath(testSourcePath / "Version/hmake0.cpp", testSourcePath / "hmake.cpp");
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("app/");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/app", "Hello World\n");
    current_path("../");

    executeSnapshotBalances(Updates{});

    // Touching main.cpp.
    path mainFilePath = testSourcePath / "main.cpp";
    touchFile(mainFilePath);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1});

    // Touching main.cpp. But hbuild executed in app-cpp.
    touchFile(mainFilePath);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1}, "app-cpp/");

    // Now executing again in Build
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1});

    // Touching main.cpp. But hbuild executed in app
    touchFile(mainFilePath);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1}, "app/");

    // Deleting app.exe
    path appExeFilePath =
        testSourcePath / "Build/app" / path(getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"));
    removeFilePath(appExeFilePath);
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1});

    // Deleting app.exe. But hbuild executed in app-cpp first and then in app
    removeFilePath(appExeFilePath);
    executeSnapshotBalances(Updates{}, "app-cpp/");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1}, "app/");

    // Deleting app-cpp.cache
    path appCppCacheFilePath = testSourcePath / "Build/app-cpp/Cache_Build_Files/app-cpp.cache";
    removeFilePath(appCppCacheFilePath);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1});

    // Deleting app-cpp.cache but executing hbuild in app
    removeFilePath(appCppCacheFilePath);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1}, "app/");

    // Deleting main.cpp.o
    path mainDotCppDotOFilePath = testSourcePath / "Build/app-cpp/Cache_Build_Files/main.cpp.o";
    removeFilePath(mainDotCppDotOFilePath);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1});

    // Deleting main.cpp.o but executing in app/
    removeFilePath(mainDotCppDotOFilePath);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1}, "app/");

    // Updating compiler-flags
    copyFilePath(testSourcePath / "Version/hmake1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1});

    // Updating compiler-flags but executing in app
    copyFilePath(testSourcePath / "Version/hmake0.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1});

    // Updating compiler-flags but executing in app-cpp
    copyFilePath(testSourcePath / "Version/hmake1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1}, "app-cpp/");

    // Executing in Build. Only app to be updated.
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1});
}

static void setupTest2Default()
{
    path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test2");
    copyFilePath(testSourcePath / "Version/0/hmake.cpp", testSourcePath / "hmake.cpp");
    copyFilePath(testSourcePath / "Version/0/main.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/0/public-lib1.hpp", testSourcePath / "lib1/public/public-lib1.hpp");
    copyFilePath(testSourcePath / "Version/0/lib1.cpp", testSourcePath / "lib1/private/lib1.cpp");
    copyFilePath(testSourcePath / "Version/0/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");

    path extraIncludeFilePath = testSourcePath / "lib1/public/extra-include.hpp";
    if (exists(extraIncludeFilePath))
    {
        removeFilePath(extraIncludeFilePath);
    }
    path tempLib4Path = testSourcePath / "lib4/private/temp.cpp";
    if (exists(tempLib4Path))
    {
        removeFilePath(tempLib4Path);
    }
}

// Tests Property Transitiviy, rebuild in multiple directories on touching file, source-file inclusion and exclusion,
// header-files exclusion and inclusion, libraries exclusion and inclusion, caching in-case of error in
// file-compilation.

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
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsDebug = 1});

    // Touching public-lib3.hpp
    path publicLib3DotHpp = testSourcePath / "lib3/public/public-lib3.hpp";
    touchFile(publicLib3DotHpp);
    executeSnapshotBalances(Updates{.sourceFiles = 2, .cppTargets = 2, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Touching private-lib1 and main.cpp
    path privateLib1DotHpp = testSourcePath / "lib1/private/private-lib1.hpp";
    touchFile(mainFilePath);
    touchFile(privateLib1DotHpp);
    executeSnapshotBalances(Updates{.sourceFiles = 2, .cppTargets = 2, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching lib4.cpp
    path lib4DotCpp = testSourcePath / "lib4/private/lib4.cpp";
    touchFile(lib4DotCpp);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching public-lib4.hpp
    path publicLib4DotHpp = testSourcePath / "lib4/public/public-lib4.hpp";
    touchFile(publicLib4DotHpp);
    executeSnapshotBalances(Updates{.sourceFiles = 3, .cppTargets = 3, .linkTargetsNoDebug = 3, .linkTargetsDebug = 1});

    // Deleting lib3-cpp.cache
    path lib3CppCacheFilePath = testSourcePath / "Build/Debug/lib3-cpp/Cache_Build_Files/lib3-cpp.cache";
    removeFilePath(lib3CppCacheFilePath);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Deleting lib4 and lib2's app-cpp.cache
    path lib4 = testSourcePath / "Build/Debug/lib4/" /
                path(getActualNameFromTargetName(TargetType::LIBRARY_STATIC, os, "lib4"));
    path lib2CppCacheFilePath = testSourcePath / "Build/Debug/lib2-cpp/Cache_Build_Files/lib2-cpp.cache";
    removeFilePath(lib4);
    removeFilePath(lib2CppCacheFilePath);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Touching main.cpp lib1.cpp lib1.hpp-public lib4.hpp-public
    path lib1DotCpp = testSourcePath / "lib1/private/lib1.cpp";
    path publicLib1DotHpp = testSourcePath / "lib1/public/public-lib1.hpp";
    touchFile(mainFilePath);
    touchFile(lib1DotCpp);
    touchFile(publicLib1DotHpp);
    touchFile(publicLib4DotHpp);
    executeSnapshotBalances(Updates{.sourceFiles = 5, .cppTargets = 5, .linkTargetsNoDebug = 4, .linkTargetsDebug = 1});

    // Touching public-lib4 then running hbuild in lib4-cpp, lib3-cpp, lib3, Build
    touchFile(publicLib4DotHpp);
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1}, "Debug/lib3");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Touching lib2.cpp, then executing in lib4, lib3-cpp, lib3, lib1, lib1-cpp, app
    path lib2DotCpp = testSourcePath / "lib2/private/lib2.cpp";
    touchFile(lib2DotCpp);
    executeSnapshotBalances(Updates{}, "Debug/lib4");
    executeSnapshotBalances(Updates{}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{}, "Debug/lib3");
    executeSnapshotBalances(Updates{}, "Debug/lib1");
    executeSnapshotBalances(Updates{}, "Debug/lib1-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1},
                            "Debug/app");

    // Touching main.cpp lib1.hpp-public, then hbuild in app
    touchFile(mainFilePath);
    touchFile(publicLib1DotHpp);
    executeSnapshotBalances(Updates{.sourceFiles = 2, .cppTargets = 2, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1},
                            "Debug/app");

    // Adding public-lib1.hpp contents to main.cpp and lib1.cpp and removing it from directory
    copyFilePath(testSourcePath / "Version/1/main.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/1/lib1.cpp", testSourcePath / "lib1/private/lib1.cpp");
    removeFilePath(testSourcePath / "lib1/public/public-lib1.hpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1}, "Debug/lib1-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1},
                            "Debug/app");
    executeSnapshotBalances(Updates{});

    // Replacing public-lib1.hpp with two header-files and restoring lib1.cpp and main.cpp
    copyFilePath(testSourcePath / "Version/0/main.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/0/lib1.cpp", testSourcePath / "lib1/private/lib1.cpp");
    copyFilePath(testSourcePath / "Version/2/public-lib1.hpp", testSourcePath / "lib1/public/public-lib1.hpp");
    copyFilePath(testSourcePath / "Version/2/extra-include.hpp", testSourcePath / "lib1/public/extra-include.hpp");
    executeSnapshotBalances(Updates{}, "Debug/lib2-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1}, "Debug/lib1-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Resorting to the default-version for the project
    setupTest2Default();
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";

    // Removing all libraries, making main simple and reconfiguring the project.
    copyFilePath(testSourcePath / "Version/3/main.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/3/hmake.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";

    executeSnapshotBalances(Updates{}, "Debug/lib2-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1}, "Debug/app-cpp");
    executeSnapshotBalances(Updates{.linkTargetsDebug = 1}, "Debug/app");

    // Resorting to the old-main and reconfiguring the project.
    copyFilePath(testSourcePath / "Version/0/main.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/0/hmake.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";

    executeSnapshotBalances(Updates{}, "Debug/lib2-cpp");
    executeSnapshotBalances(Updates{}, "Debug/lib4");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsDebug = 1});
    // Moving lib4.cpp code to temp.cpp in lib4/
    removeFilePath(testSourcePath / "lib4/private/lib4.cpp");
    copyFilePath(testSourcePath / "Version/4/temp.cpp", testSourcePath / "lib4/private/temp.cpp");
    executeSnapshotBalances(Updates{}, "Debug/lib2-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1}, "Debug/lib4");
    executeSnapshotBalances(Updates{.linkTargetsDebug = 1});

    // Copying an erroneous lib4.cpp to lib4/private. Also touching temp.cpp and removing lib3.cpp
    copyFilePath(testSourcePath / "Version/5/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    touchFile(testSourcePath / "lib4/private/temp.cpp");
    removeFilePath(testSourcePath / "Build/Debug/lib3/" /
                   getActualNameFromTargetName(TargetType::LIBRARY_STATIC, os, "lib3"));
    executeErroneousSnapshotBalances(
        Updates{.errorFiles = 1, .sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1});
    executeErroneousSnapshotBalances(Updates{.errorFiles = 1, .cppTargets = 1});
    executeSnapshotBalances(Updates{}, "Debug/lib3");

    // Erroneous lib4.cpp replaced by an empty lib4.cpp
    copyFilePath(testSourcePath / "Version/6/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    executeSnapshotBalances(Updates{}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Copying Erroneous lib4.cpp to lib4/private and changing the hmake.cpp and reconfiguring the project.
    copyFilePath(testSourcePath / "Version/5/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    copyFilePath(testSourcePath / "Version/7/hmake.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";

    executeErroneousSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1},
                                     "Release/lib3/");
    executeErroneousSnapshotBalances(Updates{}, "Release/lib3/");
    executeErroneousSnapshotBalances(Updates{.errorFiles = 1, .sourceFiles = 1, .cppTargets = 1}, "Release/lib4/");
    executeErroneousSnapshotBalances(
        Updates{.errorFiles = 1, .sourceFiles = 3, .cppTargets = 4, .linkTargetsNoDebug = 2});
    executeErroneousSnapshotBalances(Updates{.errorFiles = 1, .cppTargets = 1});

    // Copying Correct lib4.cpp
    copyFilePath(testSourcePath / "Version/6/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    executeSnapshotBalances(Updates{}, "Release/lib3/");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 2});
}

static void setupTest3Default()
{
    path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test3");
    copyFilePath(testSourcePath / "Version/0/hmake.cpp", testSourcePath / "hmake.cpp");
    copyFilePath(testSourcePath / "Version/0/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    copyFilePath(testSourcePath / "Version/0/lib3.cpp", testSourcePath / "lib3/private/lib3.cpp");
    copyFilePath(testSourcePath / "Version/0/lib2.cpp", testSourcePath / "lib2/private/lib2.cpp");

    path extraIncludeFilePath = testSourcePath / "lib1/public/extra-include.hpp";
    if (exists(extraIncludeFilePath))
    {
        removeFilePath(extraIncludeFilePath);
    }
    path tempLib4Path = testSourcePath / "lib4/private/temp.cpp";
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
    path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test3");
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
    executeSnapshotBalances(Updates{.smruleFiles = 2, .cppTargets = 1}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching public-lib3.hpp
    path publicLib3DotHpp = testSourcePath / "lib3/public/public-lib3.hpp";
    touchFile(publicLib3DotHpp);
    executeSnapshotBalances(Updates{.smruleFiles = 1, .cppTargets = 1}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1}, "Debug/lib2");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching public-lib4.hpp
    path publicLib4DotHpp = testSourcePath / "lib4/public/public-lib4.hpp";
    touchFile(publicLib4DotHpp);
    executeSnapshotBalances(Updates{.smruleFiles = 1, .cppTargets = 1}, "Debug/lib1-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1}, "Debug/lib2-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1}, "Debug/lib2");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Making lib4.cpp and lib2.cpp both module-files.
    // 4 smrules files lib4.cpp lib3.cpp public-lib4.hpp privte-lib4.hpp will be updated
    copyFilePath(testSourcePath / "Version/3/hmake.cpp", testSourcePath / "hmake.cpp");
    copyFilePath(testSourcePath / "Version/3/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    copyFilePath(testSourcePath / "Version/3/lib2.cpp", testSourcePath / "lib2/private/lib2.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.smruleFiles = 4, .cppTargets = 2}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 2}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Removing both header-units from lib4.cpp
    copyFilePath(testSourcePath / "Version/4/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    executeSnapshotBalances(Updates{.smruleFiles = 1, .cppTargets = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Adding both header-units back in lib4.cpp
    copyFilePath(testSourcePath / "Version/3/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    executeSnapshotBalances(Updates{.smruleFiles = 1, .cppTargets = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching public-lib4.hpp.
    // lib3.cpp has a header-unit dep on public-lib3.hpp which has a header-dep on public-lib4.hpp, i.e. public-lib3.hpp
    // will be recompiled and its dependent lib3.cpp will also be recompiled but public-lib4.hpp is itself a header-unit
    // in lib4.cpp, so its smrule will also be generated.
    touchFile(testSourcePath / "lib4/public/public-lib4.hpp");
    executeSnapshotBalances(Updates{.smruleFiles = 2, .sourceFiles = 1, .moduleFiles = 1, .cppTargets = 2},
                            "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{}, "Debug/lib1-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 1, .linkTargetsNoDebug = 1}, "Debug/lib2");
    executeSnapshotBalances(Updates{.sourceFiles = 1}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Changing hmake.cpp to have option to switch lib4.cpp from module to source
    copyFilePath(testSourcePath / "Version/4/hmake.cpp", testSourcePath / "hmake.cpp");
    copyFilePath(testSourcePath / "Version/4/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.smruleFiles = 3,
                                    .sourceFiles = 2,
                                    .moduleFiles = 2,
                                    .cppTargets = 1,
                                    .linkTargetsNoDebug = 2,
                                    .linkTargetsDebug = 1});
    executeSnapshotBalances(Updates{});
    path cacheFile = testSourcePath / "Build/cache.json";
    Json cacheJson;
    ifstream(cacheFile) >> cacheJson;
    // Moving from module to source, lib4.cpp will be recompiled because lib4.cpp.o was not compiled with latest
    // changes. Other file is lib2.cpp.o which is being recompiled because public-lib4.hpp is being recompiled because
    // of change of compile-command due to removal of compile-definition.
    cacheJson["cache-variables"]["use-module"] = false;
    ofstream(cacheFile) << cacheJson.dump(4);
    executeSnapshotBalances(Updates{.smruleFiles = 1,
                                    .sourceFiles = 2,
                                    .moduleFiles = 1,
                                    .cppTargets = 1,
                                    .linkTargetsNoDebug = 2,
                                    .linkTargetsDebug = 1});
    cacheJson["cache-variables"]["use-module"] = true;
    ofstream(cacheFile) << cacheJson.dump(4);
    executeSnapshotBalances(Updates{.smruleFiles = 1, .cppTargets = 1}, "Debug/lib3");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 1, .linkTargetsNoDebug = 1}, "Debug/lib2");
    // The following line is commented because when moving from source to modules, a linker-error happens. But relinking
    // does not cause the error.
    // executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1});
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    // Moving back to source from module. lib4.cpp.o should not be rebuilt because lib4.cpp with the same
    // compile-command is already in the cache but relinking should happen because previously it were source-file
    // object-files, now it is module-file object-files.
    cacheJson["cache-variables"]["use-module"] = false;
    ofstream(cacheFile) << cacheJson.dump(4);
    executeSnapshotBalances(Updates{.smruleFiles = 1,
                                    .sourceFiles = 1,
                                    .moduleFiles = 1,
                                    .cppTargets = 1,
                                    .linkTargetsNoDebug = 2,
                                    .linkTargetsDebug = 1});
}

#endif

// TODO
// Few features like PrebuiltLinkOrArchiveTarget::outputName and PrebuiltLinkOrArchiveTarget::name aren't tested. atm.
// Testing could be further expanded as-well.