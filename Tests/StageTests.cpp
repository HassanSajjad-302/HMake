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

static void noFileUpdated(const path &hbuildExecutionPath = current_path(), const path &snapshotPath = current_path())
{
    // Running configure.exe --build should not update any file
    path p = current_path();
    current_path(hbuildExecutionPath);
    Snapshot snapshot(snapshotPath);
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    snapshot.after(snapshotPath);
    current_path(p);
    ASSERT_EQ(snapshot.snapshotBalancesTest1(false, false), true);
}

static void executeSubTest1(Test1Setup setup)
{
    // Running configure.exe --build should not update any file
    path p = current_path();
    current_path(setup.hbuildExecutionPath);
    Snapshot snapshot(setup.snapshotPath);
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    snapshot.after(setup.snapshotPath);
    current_path(p);
    ASSERT_EQ(snapshot.snapshotBalancesTest1(setup.sourceFileUpdated, setup.executableFileUpdated), true);
}

/*static Snapshot getSnapshotSubTest1(const path &hbuildExecutionPath = current_path(),
                                    const path &snapshotPath = current_path())
{
    // Running configure.exe --build should not update any file
    path p = current_path();
    current_path(hbuildExecutionPath);
    Snapshot snapshot(snapshotPath);
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    snapshot.after(snapshotPath);
    current_path(p);
    ASSERT_EQ(snapshot.snapshotBalancesTest1(false, false), true);
}*/

// Tests Hello-World and rebuild in different directories on touching file.
TEST(StageTests, Test1)
{
    path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test1");
    current_path(testSourcePath);
    copyFilePath(testSourcePath / "Version/hmake0.cpp", testSourcePath / "hmake.cpp");
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getSlashedExecutableName("app"), "Hello World\n");
    current_path("../");

    noFileUpdated();

    Snapshot snapshot(current_path());

    // Touching main.cpp. Running app using configure --build.
    path mainFilePath = testSourcePath / "main.cpp";
    touchFile(mainFilePath);
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest1(true, true), true);

    executeSubTest1(Test1Setup{});

    // Touching main.cpp. But hbuild executed in app-cpp.
    touchFile(mainFilePath);
    executeSubTest1(Test1Setup{.hbuildExecutionPath = "app-cpp/", .sourceFileUpdated = true});
    executeSubTest1(Test1Setup{.hbuildExecutionPath = "app-cpp/"});

    // Now executing again in Build
    executeSubTest1(Test1Setup{.executableFileUpdated = true});
    executeSubTest1(Test1Setup{});

    // Touching main.cpp. But hbuild executed in app
    touchFile(mainFilePath);
    executeSubTest1(
        Test1Setup{.hbuildExecutionPath = "app/", .sourceFileUpdated = true, .executableFileUpdated = true});
    executeSubTest1(Test1Setup{.hbuildExecutionPath = "app/"});
    executeSubTest1(Test1Setup{});

    // Deleting app.exe
    path appExeFilePath =
        testSourcePath / "Build/app" / path(getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"));
    removeFilePath(appExeFilePath);
    executeSubTest1(Test1Setup{.executableFileUpdated = true});
    executeSubTest1(Test1Setup{});

    // Deleting app.exe. But hbuild executed in app-cpp first and then in app
    removeFilePath(appExeFilePath);
    executeSubTest1(Test1Setup{.hbuildExecutionPath = "app-cpp/"});
    executeSubTest1(Test1Setup{.hbuildExecutionPath = "app/", .executableFileUpdated = true});
    executeSubTest1(Test1Setup{});

    // Deleting app-cpp.cache
    path appCppCacheFilePath = testSourcePath / "Build/app-cpp/Cache_Build_Files/app-cpp.cache";
    removeFilePath(appCppCacheFilePath);
    executeSubTest1(Test1Setup{.sourceFileUpdated = true, .executableFileUpdated = true});
    executeSubTest1(Test1Setup{});

    // Deleting app-cpp.cache but executing hbuild in app
    removeFilePath(appCppCacheFilePath);
    executeSubTest1(
        Test1Setup{.hbuildExecutionPath = "app/", .sourceFileUpdated = true, .executableFileUpdated = true});
    executeSubTest1(Test1Setup{});

    // Deleting main.cpp.o
    path mainDotCppDotOFilePath = testSourcePath / "Build/app-cpp/Cache_Build_Files/main.cpp.o";
    removeFilePath(mainDotCppDotOFilePath);
    executeSubTest1(Test1Setup{.sourceFileUpdated = true, .executableFileUpdated = true});
    executeSubTest1(Test1Setup{});

    // Deleting main.cpp.o but executing in app/
    removeFilePath(mainDotCppDotOFilePath);
    executeSubTest1(
        Test1Setup{.hbuildExecutionPath = "app/", .sourceFileUpdated = true, .executableFileUpdated = true});
    executeSubTest1(Test1Setup{});

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

// Tests Property Transitiviy, rebuild in multiple directories on touching file, source-file inclusion and exclusion,
// header-files exclusion and inclusion, libraries exclusion and inclusion.
/*
TEST(StageBasicTests, Test2)
{
    path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test2");
    current_path(testSourcePath);
    ExamplesTestHelper::recreateBuildDirAndBuildHMakeProject();
    current_path("Debug/app/");
    ExamplesTestHelper::runAppWithExpectedOutput(getSlashedExecutableName("app"), "36\n");
    current_path("../../");

    noFileUpdated();

    Snapshot snapshot(current_path());

    // Touching main.cpp
    path mainFilePath = testSourcePath / "main.cpp";
    touchFile(mainFilePath);
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest2(Test2Touched{.mainDotCpp = true}), true);

    noFileUpdated();

    // Touching public-lib3.hpp
    path publicLib3DotHpp = testSourcePath / "lib3/public/public-lib3.hpp";
    touchFile(publicLib3DotHpp);
    snapshot.before(current_path());
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest2(Test2Touched{.publicLib3DotHpp = true}), true);

    noFileUpdated();

    // private-lib1 main.cpp
    path privateLib1DotHpp = testSourcePath / "lib1/private/private-lib1.hpp";
    touchFile(privateLib1DotHpp);
    snapshot.before(current_path());
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest2(Test2Touched{.privateLib1DotHpp = true}), true);

    noFileUpdated();

    // Touching lib4.cpp
    path lib4DotCpp = testSourcePath / "lib4/private/lib4.cpp";
    touchFile(lib4DotCpp);
    snapshot.before(current_path());
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest2(Test2Touched{.lib4DotCpp = true}), true);

    noFileUpdated();

    // Touching public-lib4.hpp
    path publicLib4DotHpp = testSourcePath / "lib4/public/public-lib4.hpp";
    touchFile(publicLib4DotHpp);
    snapshot.before(current_path());
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest2(Test2Touched{.publicLib4DotHpp = true}), true);

    noFileUpdated();

    // Deleting app-cpp.cache
    path lib3CppCacheFilePath = testSourcePath / "Build/Debug/lib3-cpp/Cache_Build_Files/lib3-cpp.cache";
    snapshot.before(current_path());
    removeFilePath(lib3CppCacheFilePath);
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest2(Test2Touched{.lib3DotCpp = true}), true);

    noFileUpdated();

    // Deleting lib4 and lib2's app-cpp.cache
    path lib4 = testSourcePath / "Build/Debug/lib4/" /
                path(getActualNameFromTargetName(TargetType::LIBRARY_STATIC, os, "lib4"));
    path lib2CppCacheFilePath = testSourcePath / "Build/Debug/lib2-cpp/Cache_Build_Files/lib2-cpp.cache";
    snapshot.before(current_path());
    removeFilePath(lib4);
    removeFilePath(lib2CppCacheFilePath);
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest2(Test2Touched{.lib2DotCpp = true, .lib4Linked = true}), true);

    noFileUpdated();

    // Touching main.cpp lib1.cpp lib1.hpp-public lib4.hpp-public
    path lib1DotCpp = testSourcePath / "lib1/private/lib1.cpp";
    path publicLib1DotHpp = testSourcePath / "lib1/public/public-lib1.hpp";
    touchFile(mainFilePath);
    touchFile(lib1DotCpp);
    touchFile(publicLib1DotHpp);
    touchFile(publicLib4DotHpp);
    snapshot.before(current_path());
    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalancesTest2(
                  Test2Touched{.mainDotCpp = true, .publicLib1DotHpp = true, .publicLib4DotHpp = true}),
              true);

    noFileUpdated();

    // Touching public-lib4 then running hbuild in lib4-cpp, lib3-cpp, lib3, Build
    touchFile(publicLib4DotHpp);
    snapshot.before(current_path());
    current_path("Debug/lib4-cpp");
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    current_path("../../");
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalances(1, 1, 0, 0), true);
    snapshot.before(current_path());
    current_path("Debug/lib3-cpp");
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    current_path("../../");
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalances(1, 1, 0, 0), true);
    snapshot.before(current_path());
    current_path("Debug/lib3/");
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    current_path("../../");
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalances(0, 0, 1, 0), true);
    snapshot.before(current_path());
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
    snapshot.after(current_path());
    ASSERT_EQ(snapshot.snapshotBalances(1, 1, 2, 1), true);

    // TODO
    // noFileUpdated() function call with current_path whenever in some path.

    noFileUpdated();

    // Touching lib2.cpp, then executing in lib4, lib3-cpp, lib3, lib1, lib1-cpp, app
    path lib2DotCpp = testSourcePath / "lib1/private/lib2.cpp";
    touchFile(lib2DotCpp);
    noFileUpdated("Debug/lib4");
    noFileUpdated("Debug/lib3-cpp");
    noFileUpdated("Debug/lib3");

    */
/*    ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        snapshot.after(current_path());
        ASSERT_EQ(snapshot.snapshotBalancesTest2(
                      Test2Touched{.mainDotCpp = true, .publicLib1DotHpp = true, .publicLib4DotHpp = true}),
                  true);

        noFileUpdated();

        // Touching main.cpp lib1.hpp-public, then hbuild in app
        path lib1DotCpp = testSourcePath / "lib1/private/lib1.cpp";
        path publicLib1DotHpp = testSourcePath / "lib1/public/public-lib1.hpp";
        touchFile(mainFilePath);
        touchFile(lib1DotCpp);
        touchFile(publicLib1DotHpp);
        touchFile(publicLib4DotHpp);
        ASSERT_EQ(system(configureBuildStr.c_str()), 0) << configureBuildStr + " command failed.";
        snapshot.after(current_path());
        ASSERT_EQ(snapshot.snapshotBalancesTest2(
                      Test2Touched{.mainDotCpp = true, .publicLib1DotHpp = true, .publicLib4DotHpp = true}),
                  true);

        noFileUpdated();*//*

}*/
