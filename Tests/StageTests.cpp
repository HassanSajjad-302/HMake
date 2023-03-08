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
    if constexpr (os == OS::NT)
    {
        // TODO
        // On Windows copying does not edit the last-update-time. Not investing further atm.
        touchFile(destinationFilePath);
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

static void executeErroneousSnapshotBalances(unsigned short errorFiles, unsigned short filesCompiled,
                                             unsigned short cppTargets, unsigned short linkTargetsNoDebug,
                                             unsigned short linkTargetsDebug,
                                             const path &hbuildExecutionPath = current_path())
{
    // Running configure.exe --build should not update any file
    path p = current_path();
    current_path(hbuildExecutionPath);
    Snapshot snapshot(p);
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    snapshot.after(p);
    ASSERT_EQ(
        snapshot.snapshotErroneousBalances(errorFiles, filesCompiled, cppTargets, linkTargetsNoDebug, linkTargetsDebug),
        true);
    current_path(p);
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
    ExamplesTestHelper::runAppWithExpectedOutput("app", "Hello World\n");
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
// header-files exclusion and inclusion, libraries exclusion and inclusion.
TEST(StageTests, Test2)
{
    path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test2");
    current_path(testSourcePath);
    setupTest2Default();

    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("Debug/app/");
    ExamplesTestHelper::runAppWithExpectedOutput("app", "36\n");
    current_path("../../");

    executeSubTest2(Test2Setup{});
    /*

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

        // Adding public-lib1.hpp contents to main.cpp and lib1.cpp and removing it from directory
        copyFilePath(testSourcePath / "Version/1/main.cpp", testSourcePath / "main.cpp");
        copyFilePath(testSourcePath / "Version/1/lib1.cpp", testSourcePath / "lib1/private/lib1.cpp");
        removeFilePath(testSourcePath / "lib1/public/public-lib1.hpp");
        executeSnapshotBalances(1, 1, 0, 0, "Debug/lib1-cpp");
        executeSnapshotBalances(1, 1, 1, 1, "Debug/app");
        executeSnapshotBalances(0, 0, 0, 0);

        // Replacing public-lib1.hpp with two header-files and restoring lib1.cpp and main.cpp
        copyFilePath(testSourcePath / "Version/0/main.cpp", testSourcePath / "main.cpp");
        copyFilePath(testSourcePath / "Version/0/lib1.cpp", testSourcePath / "lib1/private/lib1.cpp");
        copyFilePath(testSourcePath / "Version/2/public-lib1.hpp", testSourcePath / "lib1/public/public-lib1.hpp");
        copyFilePath(testSourcePath / "Version/2/extra-include.hpp", testSourcePath / "lib1/public/extra-include.hpp");
        executeSnapshotBalances(0, 0, 0, 0, "Debug/lib2-cpp");
        executeSnapshotBalances(1, 1, 0, 0, "Debug/lib1-cpp");
        executeSnapshotBalances(1, 1, 1, 1);
    */

    // Resorting to the default-version for the project
    setupTest2Default();
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";

    // Removing all libraries, making main simple and reconfiguring the project.
    copyFilePath(testSourcePath / "Version/3/main.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/3/hmake.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";

    executeSnapshotBalances(0, 0, 0, 0, "Debug/lib2-cpp");
    executeSnapshotBalances(1, 1, 0, 0, "Debug/app-cpp");
    executeSnapshotBalances(0, 0, 0, 1, "Debug/app");

    // Resorting to the old-main and reconfiguring the project.
    copyFilePath(testSourcePath / "Version/0/main.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/0/hmake.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";

    executeSnapshotBalances(0, 0, 0, 0, "Debug/lib2-cpp");
    executeSnapshotBalances(0, 0, 0, 0, "Debug/lib4");
    executeSnapshotBalances(1, 1, 0, 1);

    // Moving lib4.cpp code to temp.cpp in lib4/
    removeFilePath(testSourcePath / "lib4/private/lib4.cpp");
    copyFilePath(testSourcePath / "Version/4/temp.cpp", testSourcePath / "lib4/private/temp.cpp");
    executeSnapshotBalances(0, 0, 0, 0, "Debug/lib2-cpp");
    executeSnapshotBalances(1, 1, 1, 0, "Debug/lib4");
    executeSnapshotBalances(0, 0, 0, 1);

    // Copying an erroneous lib4.cpp to lib4/private. Also touching temp.cpp and removing lib3.cpp
    copyFilePath(testSourcePath / "Version/5/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    touchFile(testSourcePath / "lib4/private/temp.cpp");
    removeFilePath(testSourcePath / "Build/Debug/lib3/" /
                   getActualNameFromTargetName(TargetType::LIBRARY_STATIC, os, "lib3"));
    executeErroneousSnapshotBalances(1, 1, 1, 1, 0);
    executeErroneousSnapshotBalances(1, 0, 1, 0, 0);
    executeSnapshotBalances(0, 0, 0, 0, "Debug/lib3");

    // Erroneous lib4.cpp replaced by an empty lib4.cpp
    copyFilePath(testSourcePath / "Version/6/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    executeSnapshotBalances(0, 0, 0, 0, "Debug/lib3-cpp");
    executeSnapshotBalances(1, 1, 0, 0, "Debug/lib4-cpp");
    executeSnapshotBalances(0, 0, 1, 1);

    // Copying Erroneous lib4.cpp to lib4/private and changing the hmake.cpp and reconfiguring the project.
    copyFilePath(testSourcePath / "Version/5/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    copyFilePath(testSourcePath / "Version/7/hmake.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";

    executeErroneousSnapshotBalances(0, 1, 1, 1, 0, "Debug/lib3/");
    executeErroneousSnapshotBalances(0, 0, 0, 0, 0, "Debug/lib3/");
    executeErroneousSnapshotBalances(1, 1, 1, 0, 0, "Debug/lib4/");
    executeErroneousSnapshotBalances(1, 3, 4, 2, 0);
    executeErroneousSnapshotBalances(1, 0, 1, 0, 0);

    // Copying Correct lib4.cpp
    copyFilePath(testSourcePath / "Version/6/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    executeSnapshotBalances(0, 0, 0, 0, "Debug/lib3/");
    executeSnapshotBalances(1, 1, 2, 0);
}
