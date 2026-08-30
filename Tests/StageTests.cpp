#include "BuildSystemFunctions.hpp"
#include "CppMod.hpp"
#include "ExamplesTestHelper.hpp"
#include "Features.hpp"
#include "Snapshot.hpp"
#include "gtest/gtest.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

using std::string, std::ofstream, std::ifstream, std::filesystem::create_directory, std::filesystem::create_directories,
    std::filesystem::path, std::cout, std::format, std::filesystem::remove_all, std::ifstream, std::ofstream,
    std::filesystem::remove, std::filesystem::copy_file, std::error_code, std::filesystem::copy_options, std::print;

static void touchFile(const path &filePath)
{
    std::ofstream file(filePath, std::ios::app);
    if (!file)
    {
        printErrorMessage(FORMAT("Test setup could not open a file for touching.\nPath: {}", filePath.string()));
        return;
    }
    file << '\n';
}

static void removeFilePath(const path &filePath, bool removeDirContents = false)
{
    if (removeDirContents)
    {
        for (const auto &c : std::filesystem::directory_iterator(filePath))
        {
            error_code ec;
            if (const bool removed = remove(c, ec); !removed || ec)
            {
                printErrorMessage(FORMAT("Test cleanup could not remove a directory entry.\nPath: {}\nSystem error: {}",
                                         c.path().string(), ec ? ec.message() : "unknown error"));
            }
        }
        return;
    }

    error_code ec;
    if (const bool removed = remove(filePath, ec); !removed || ec)
    {
        printErrorMessage(FORMAT("Test cleanup could not remove a file.\nPath: {}\nSystem error: {}", filePath.string(),
                                 ec ? ec.message() : "unknown error"));
    }
}

static void removeDirectory(const path &filePath)
{
    error_code ec;
    if (const bool removed = remove_all(filePath, ec); !removed || ec)
    {
        printErrorMessage(FORMAT("Test cleanup could not remove a directory tree.\nPath: {}\nSystem error: {}",
                                 filePath.string(), ec ? ec.message() : "unknown error"));
    }
}

static void copyFilePath(const path &sourceFilePath, const path &destinationFilePath)
{
    error_code ec;
    if (const bool copied = copy_file(sourceFilePath, destinationFilePath, copy_options::overwrite_existing, ec);
        !copied || ec)
    {
        printErrorMessage(FORMAT("Test setup could not copy a file.\nSource: {}\nDestination: {}\nSystem error: {}",
                                 sourceFilePath.string(), destinationFilePath.string(),
                                 ec ? ec.message() : "unknown error"));
    }
    if constexpr (os == OS::NT)
    {
        // TODO
        // On Windows copying does not edit the last-update-time. Not investing further atm.
        touchFile(destinationFilePath);
    }
}

// macro needed to ensure early exit from the tests if executeSnapshotBalances fails

#define BALANCES(...) ASSERT_NO_FATAL_FAILURE(executeSnapshotBalances(__VA_ARGS__))

#include <stacktrace>
static void executeSnapshotBalances(const Updates &updates, const path &hbuildExecutionPath = current_path())
{
    const path p = current_path();
    current_path(hbuildExecutionPath);
    Snapshot snapshot(p);

    {
        const auto result = RunCommand::runProcess(hbuildBuildStr);
        printMessage(result.output);
        ASSERT_EQ(result.exitStatus, 0) << hbuildBuildStr + " command failed.";
    }

    snapshot.after(p);
    ASSERT_EQ(snapshot.snapshotBalances(updates), true);

    snapshot.before(p);

    {
        const auto result = RunCommand::runProcess(hbuildBuildStr);
        printMessage(result.output);
        ASSERT_EQ(result.exitStatus, 0) << hbuildBuildStr + " command failed.";
    }

    snapshot.after(p);
    current_path(p);
    ASSERT_EQ(snapshot.snapshotBalances(Updates{}), true);
}

// macro needed to ensure early exit from the tests if executeErroneousSnapshotBalances fails

#define ERROR_BALANCES(...) ASSERT_NO_FATAL_FAILURE(executeErroneousSnapshotBalances(__VA_ARGS__))

static void executeErroneousSnapshotBalances(const Updates &updates, const path &hbuildExecutionPath = current_path())
{
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
    copyFilePath(testSourcePath / "Version/hmakev0.cpp", testSourcePath / "hmake.cpp");
    ExamplesTestHelper::cleanBuild();
    current_path("Release/app/");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/app", "Hello World\n");
    current_path("../../");

    BALANCES(Updates{});

    // Touching main.cpp.
    const path mainFilePath = testSourcePath / "main.cpp";
    touchFile(mainFilePath);
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .nodesFile = true});

    // Touching main.cpp. But hbuild executed in app-cpp.
    touchFile(mainFilePath);
    BALANCES(Updates{.sourceFiles = 1, .nodesFile = true}, "Release/app-cpp/");

    // Now executing again in Build
    BALANCES(Updates{.linkTargetsNoDebug = 1});

    // Touching main.cpp. But hbuild executed in app
    touchFile(mainFilePath);
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .nodesFile = true}, "Release/app/");

    // Deleting app.exe
    const path appExeFilePath =
        testSourcePath / "Build/Release/app" / path(getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"));
    removeFilePath(appExeFilePath);
    BALANCES(Updates{.linkTargetsNoDebug = 1});

    // Deleting app.exe. But hbuild executed in app-cpp first and then in app
    removeFilePath(appExeFilePath);
    BALANCES(Updates{}, "Release/app-cpp/");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Release/app/");

    // Deleting app-cpp dir
    const path appCppDirectory = testSourcePath / "Build/Release/app-cpp/";
    removeDirectory(appCppDirectory);
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1});

    // Deleting app-cpp dir but executing hbuild in app
    removeDirectory(appCppDirectory);
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1}, "Release/app/");

    // Deleting main.cpp.o
    const path appCppDir = testSourcePath / "Build/Release/app-cpp";
    removeFilePath(appCppDir, true);
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1});

    // Deleting main.cpp.o but executing in app/
    removeFilePath(appCppDir, true);
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1}, "Release/app/");

    // Updating compiler-flags
    copyFilePath(testSourcePath / "Version/hmakev1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1});

    // Updating compiler-flags but executing in app
    copyFilePath(testSourcePath / "Version/hmakev0.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1});

    // Updating compiler-flags but executing in app-cpp
    copyFilePath(testSourcePath / "Version/hmakev1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.sourceFiles = 1}, "Release/app-cpp/");

    // Executing in Build. Only app to be updated.
    BALANCES(Updates{.linkTargetsNoDebug = 1});
}

static void setupTest2Default()
{
    const path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test2");
    copyFilePath(testSourcePath / "Version/hmakev0.cpp", testSourcePath / "hmake.cpp");
    copyFilePath(testSourcePath / "Version/mainv0.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/public-lib1v0.hpp", testSourcePath / "lib1/public/public-lib1.hpp");
    copyFilePath(testSourcePath / "Version/lib1v0.cpp", testSourcePath / "lib1/private/lib1.cpp");
    copyFilePath(testSourcePath / "Version/lib4v0.cpp", testSourcePath / "lib4/private/lib4.cpp");

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

// Tests Property Transitivity, rebuild in multiple dirs on touching file, source-file inclusion and exclusion,
// header-files exclusion and inclusion, libraries exclusion and inclusion, caching in-case of error in
// file-compilation.
TEST(StageTests, Test2)
{
    path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test2");
    current_path(testSourcePath);
    setupTest2Default();

    ExamplesTestHelper::cleanBuild();
    current_path("Debug/app/");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/app", "36\n");
    current_path("../../");

    BALANCES(Updates{});

    // Touching main.cpp
    path mainFilePath = testSourcePath / "main.cpp";
    touchFile(mainFilePath);
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsDebug = 1, .nodesFile = true});

    // Touching public-lib3.hpp
    path publicLib3DotHpp = testSourcePath / "lib3/public/public-lib3.hpp";
    touchFile(publicLib3DotHpp);
    BALANCES(Updates{.sourceFiles = 2, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1, .nodesFile = true});

    // Touching private-lib1 and main.cpp
    path privateLib1DotHpp = testSourcePath / "lib1/private/private-lib1.hpp";
    touchFile(mainFilePath);
    touchFile(privateLib1DotHpp);
    BALANCES(Updates{.sourceFiles = 2, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1, .nodesFile = true});

    // Touching lib4.cpp
    path lib4DotCpp = testSourcePath / "lib4/private/lib4.cpp";
    touchFile(lib4DotCpp);
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1, .nodesFile = true});

    // Touching public-lib4.hpp
    path publicLib4DotHpp = testSourcePath / "lib4/public/public-lib4.hpp";
    touchFile(publicLib4DotHpp);
    BALANCES(Updates{.sourceFiles = 3, .linkTargetsNoDebug = 3, .linkTargetsDebug = 1, .nodesFile = true});

    // Deleting lib3-cpp dir
    path lib3CppDirectory = testSourcePath / "Build/Debug/lib3-cpp/";
    removeDirectory(lib3CppDirectory);
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Deleting lib4 and lib2-cpp dir
    path lib4 = testSourcePath / "Build/Debug/lib4/" /
                path(getActualNameFromTargetName(TargetType::LIBRARY_STATIC, os, "lib4"));
    path lib2CppDirectory = testSourcePath / "Build/Debug/lib2-cpp/";
    removeFilePath(lib4);
    removeDirectory(lib2CppDirectory);
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Touching main.cpp lib1.cpp lib1.hpp-public lib4.hpp-public
    path lib1DotCpp = testSourcePath / "lib1/private/lib1.cpp";
    path publicLib1DotHpp = testSourcePath / "lib1/public/public-lib1.hpp";
    touchFile(mainFilePath);
    touchFile(lib1DotCpp);
    touchFile(publicLib1DotHpp);
    touchFile(publicLib4DotHpp);
    BALANCES(Updates{.sourceFiles = 5, .linkTargetsNoDebug = 4, .linkTargetsDebug = 1, .nodesFile = true});

    // Touching public-lib4 then running hbuild in lib4-cpp, lib3-cpp, lib3, Build
    touchFile(publicLib4DotHpp);
    BALANCES(Updates{.sourceFiles = 1, .nodesFile = true}, "Debug/lib4-cpp");
    BALANCES(Updates{.sourceFiles = 1}, "Debug/lib3-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib3");
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Touching lib2.cpp, then executing in lib4, lib3-cpp, lib3, lib1, lib1-cpp, app
    path lib2DotCpp = testSourcePath / "lib2/private/lib2.cpp";
    touchFile(lib2DotCpp);
    BALANCES(Updates{.nodesFile = true}, "Debug/lib4");
    BALANCES(Updates{}, "Debug/lib3-cpp");
    BALANCES(Updates{}, "Debug/lib3");
    BALANCES(Updates{.sourceFiles = 1}, "Debug/lib1");
    BALANCES(Updates{}, "Debug/lib1-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1}, "Debug/app");

    // Touching main.cpp lib1.hpp-public, then hbuild in app
    touchFile(mainFilePath);
    touchFile(publicLib1DotHpp);
    BALANCES(Updates{.sourceFiles = 2, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1, .nodesFile = true}, "Debug/app");

    // Adding public-lib1.hpp contents to main.cpp and lib1.cpp and removing it from dir
    copyFilePath(testSourcePath / "Version/mainv1.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/lib1v1.cpp", testSourcePath / "lib1/private/lib1.cpp");
    removeFilePath(testSourcePath / "lib1/public/public-lib1.hpp");
    BALANCES(Updates{.sourceFiles = 1, .nodesFile = true}, "Debug/lib1-cpp");
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1}, "Debug/app");
    BALANCES(Updates{});

    // Replacing public-lib1.hpp with two header-files and restoring lib1.cpp and main.cpp
    copyFilePath(testSourcePath / "Version/mainv0.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/lib1v0.cpp", testSourcePath / "lib1/private/lib1.cpp");
    copyFilePath(testSourcePath / "Version/public-lib1v1.hpp", testSourcePath / "lib1/public/public-lib1.hpp");
    copyFilePath(testSourcePath / "Version/extra-includev0.hpp", testSourcePath / "lib1/public/extra-include.hpp");
    BALANCES(Updates{.nodesFile = true}, "Debug/lib2-cpp");
    BALANCES(Updates{.sourceFiles = 1, .nodesFile = true}, "Debug/lib1-cpp");
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Resorting to the default-version for the project
    setupTest2Default();
    ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";

    // Removing all libraries, making main simple and reconfiguring the project.
    copyFilePath(testSourcePath / "Version/mainv2.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/hmakev1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";

    BALANCES(Updates{.nodesFile = true}, "Debug/lib2-cpp");
    BALANCES(Updates{.sourceFiles = 1}, "Debug/app-cpp");
    BALANCES(Updates{.linkTargetsDebug = 1}, "Debug/app");

    // Resorting to the old-main and reconfiguring the project.
    copyFilePath(testSourcePath / "Version/mainv0.cpp", testSourcePath / "main.cpp");
    copyFilePath(testSourcePath / "Version/hmakev0.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";

    BALANCES(Updates{.nodesFile = true}, "Debug/lib2-cpp");
    BALANCES(Updates{}, "Debug/lib4");
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsDebug = 1});
    // Moving lib4.cpp code to temp.cpp in lib4/
    removeFilePath(testSourcePath / "lib4/private/lib4.cpp");
    copyFilePath(testSourcePath / "Version/tempv0.cpp", testSourcePath / "lib4/private/temp.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.sourceFiles = 1, .nodesFile = true}, "Debug/lib2-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib4");
    BALANCES(Updates{.linkTargetsDebug = 1});

    // Copying an erroneous lib4.cpp to lib4/private. Also touching temp.cpp and removing lib/lib3.lib
    copyFilePath(testSourcePath / "Version/lib4v1.cpp", testSourcePath / "lib4/private/lib4.cpp");
    touchFile(testSourcePath / "lib4/private/temp.cpp");
    removeFilePath(testSourcePath / "Build/Debug/lib3/" /
                   getActualNameFromTargetName(TargetType::LIBRARY_STATIC, os, "lib3"));
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    ERROR_BALANCES(Updates{.errorFiles = 1, .sourceFiles = 1, .linkTargetsNoDebug = 1, .nodesFile = true});
    ERROR_BALANCES(Updates{.errorFiles = 1});
    ERROR_BALANCES(Updates{.errorFiles = 1}, "Debug/lib3");

    // Erroneous lib4.cpp replaced by an empty lib4.cpp
    copyFilePath(testSourcePath / "Version/lib4v2.cpp", testSourcePath / "lib4/private/lib4.cpp");
    BALANCES(Updates{.sourceFiles = 1, .nodesFile = true}, "Debug/lib4-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Copying Erroneous lib4.cpp to lib4/private and changing the hmake.cpp and reconfiguring the project.
    copyFilePath(testSourcePath / "Version/lib4v1.cpp", testSourcePath / "lib4/private/lib4.cpp");
    copyFilePath(testSourcePath / "Version/hmakev2.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";

    create_directories("Release/lib3/");
    create_directories("Release/lib4/");
    ERROR_BALANCES(Updates{.errorFiles = 1, .sourceFiles = 2, .linkTargetsNoDebug = 1, .nodesFile = true}, "Release/lib3/");
    ERROR_BALANCES(Updates{.errorFiles = 1, .sourceFiles = 3, .linkTargetsNoDebug = 2});
    ERROR_BALANCES(Updates{.errorFiles = 1});

    // Copying Empty lib4.cpp
    copyFilePath(testSourcePath / "Version/lib4v2.cpp", testSourcePath / "lib4/private/lib4.cpp");
    BALANCES(Updates{.sourceFiles = 1, .nodesFile = true}, "Release/lib4-cpp/");
    BALANCES(Updates{}, "Release/lib3-cpp/");
    BALANCES(Updates{.linkTargetsNoDebug = 2});

    // Restoring lib4.cpp. This hmake.cpp will make the selection between lib4.cpp and temp.cpp based on the cache
    // variable use-lib4.cpp value
    copyFilePath(testSourcePath / "Version/hmakev3.cpp", testSourcePath / "hmake.cpp");
    copyFilePath(testSourcePath / "Version/lib4v0.cpp", testSourcePath / "lib4/private/lib4.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.sourceFiles = 1, .nodesFile = true}, "Debug/lib2-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib4");
    BALANCES(Updates{.linkTargetsDebug = 1});

    path cacheFile = testSourcePath / "Build/cache.json";
    ifstream ifs(cacheFile);
    string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    rapidjson::Document cacheJson;
    cacheJson.Parse(content.c_str());
    ASSERT_FALSE(cacheJson.HasParseError());
    ASSERT_TRUE(cacheJson.HasMember("cache-variables"));
    ASSERT_TRUE(cacheJson["cache-variables"].HasMember("use-lib4.cpp"));
    {
        ofstream ofs(cacheFile);
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        cacheJson.Accept(writer);
        ofs << buffer.GetString();
    }
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{}, "Debug/lib2-cpp");

    // The following 2 tests are failing on Windows and I think that is due to incremental linking. Same command
    // executed on console fails and then passes.
#ifdef _WIN32
    // ASSERT_EQ(system(hbuildBuildStr.c_str()), 0) << hbuildBuildStr + " command failed.";
#else
//    BALANCES(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1}, "Debug/app");
    BALANCES(Updates{});
#endif

    // Adding a public compile definition for lib4 target. this is tested as compile-definition and compile-flags are
    // not cached like include-dirs and others.
    copyFilePath(testSourcePath / "Version/hmakev4.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.sourceFiles = 2}, "Debug/lib3-cpp");
    BALANCES(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1}, "Debug/lib2");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib4");
    BALANCES(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});
}

static void setupTest3Default(const path &testSourcePath)
{
    copyFilePath(testSourcePath / "Version/hmakev0.cpp", testSourcePath / "hmake.cpp");
    copyFilePath(testSourcePath / "Version/lib4v0.cpp", testSourcePath / "lib4/private/lib4.cpp");
    copyFilePath(testSourcePath / "Version/lib3v0.cpp", testSourcePath / "lib3/private/lib3.cpp");
    copyFilePath(testSourcePath / "Version/lib2v0.cpp", testSourcePath / "lib2/private/lib2.cpp");

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

// Tests for header-file changing to header-unit, and back from heaader-unit to header-file. Tests header-file and
// header-unit inclusion/exclusion.
TEST(StageTests, Test3)
{
    const path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test3");
    current_path(testSourcePath);
    setupTest3Default(testSourcePath);

    ExamplesTestHelper::cleanBuild();
    current_path("Debug/app/");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/app", "36\n");
    current_path("../../");

    BALANCES(Updates{});

    // Making public-lib3.hpp a header-unit
    copyFilePath(testSourcePath / "Version/hmakev1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{}, "Debug/lib4-cpp");
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 1}, "Debug/lib3-cpp");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib2-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Touching lib3.cpp
    const path publicLib3DotCpp = testSourcePath / "lib3/private/lib3.cpp";
    touchFile(publicLib3DotCpp);
    BALANCES(Updates{.nodesFile = true}, "Debug/lib4-cpp");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib3-cpp");
    BALANCES(Updates{}, "Debug/lib2");
    BALANCES(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching public-lib4.hpp
    const path publicLib4DotHpp = testSourcePath / "lib4/public/public-lib4.hpp";
    touchFile(publicLib4DotHpp);
    BALANCES(Updates{.moduleFiles = 1, .linkTargetsNoDebug = 1, .nodesFile = true}, "Debug/lib4");
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 1}, "Debug/lib3-cpp");
    BALANCES(Updates{.moduleFiles = 1, .linkTargetsNoDebug = 1}, "Debug/lib2");
    BALANCES(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching public-lib3.hpp
    const path publicLib3DotHpp = testSourcePath / "lib3/public/public-lib3.hpp";
    touchFile(publicLib3DotHpp);
    BALANCES(Updates{.nodesFile = true}, "Debug/lib4-cpp");
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 1}, "Debug/lib3-cpp");
    BALANCES(Updates{.moduleFiles = 1, .linkTargetsNoDebug = 1}, "Debug/lib2");
    BALANCES(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching public-lib4.hpp
    touchFile(publicLib4DotHpp);
    BALANCES(Updates{.moduleFiles = 1, .linkTargetsNoDebug = 1, .nodesFile = true}, "Debug/lib4");
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 1}, "Debug/lib3-cpp");
    BALANCES(Updates{.moduleFiles = 1, .linkTargetsNoDebug = 1}, "Debug/lib2");
    BALANCES(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Adding private compile-definition to lib3.
    copyFilePath(testSourcePath / "Version/hmakev3.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{}, "Debug/lib4-cpp");
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 1}, "Debug/lib3-cpp");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib2-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Removing private compile-definition lib3.cpp.
    copyFilePath(testSourcePath / "Version/hmakev1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{}, "Debug/lib4-cpp");
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 1}, "Debug/lib3-cpp");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib2-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Just an extra re-configuration test.
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{}, "Debug/lib4-cpp");

    // Making public-lib4.hpp and private-lib4.hpp header-units. compile-definition removed as well.
    copyFilePath(testSourcePath / "Version/hmakev2.cpp", testSourcePath / "hmake.cpp");
    // private-lib4.hpp, public-lib4.hpp, public-lib3.hpp, lib3.cpp, lib4.cpp.
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.headerUnits = 3, .moduleFiles = 2}, "Debug/lib3-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib3");
    BALANCES(Updates{.moduleFiles = 1, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Making public-lib4.hpp and private-lib4.hpp header-files.
    copyFilePath(testSourcePath / "Version/hmakev1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 2}, "Debug/lib3-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib4");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib3");
    BALANCES(Updates{.moduleFiles = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Making public-lib4.hpp and private-lib4.hpp header-units again. Should not be recompiled.
    copyFilePath(testSourcePath / "Version/hmakev2.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 3}, "Debug/lib1-cpp");
    BALANCES(Updates{}, "Debug/app-cpp");
    BALANCES(Updates{}, "Debug/lib1");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib2");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib3");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib4");
    BALANCES(Updates{.linkTargetsDebug = 1}, "Debug/app");

    //  Touching public-lib4.hpp.
    //  lib3.cpp has a header-unit dep on public-lib3.hpp which has a header-dep on public-lib4.hpp, i.e.
    //  public-lib3.hpp will be recompiled and its dependent lib3.cpp will also be recompiled.
    touchFile(testSourcePath / "lib4/public/public-lib4.hpp");

    // lib2.cpp, lib3.cpp, lib4.cpp, public-lib3.hpp, public-lib4.hpp.
    BALANCES(Updates{.moduleFiles = 4, .nodesFile = true}, "Debug/lib3-cpp");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib1-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib2");
    BALANCES(Updates{.linkTargetsNoDebug = 2, .linkTargetsDebug = 1});
}

// Tests for header-file changing to header-unit, and back from heaader-unit to header-file. Tests header-file and
// header-unit inclusion/exclusion.
TEST(StageTests, Test4)
{
    const path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test4");
    current_path(testSourcePath);
    setupTest3Default(testSourcePath);

    ExamplesTestHelper::cleanBuild();
    current_path("Debug/app/");
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/app", "36\n");
    current_path("../../");

    BALANCES(Updates{});

    // Making public-lib3.hpp a header-unit
    copyFilePath(testSourcePath / "Version/hmakev1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.headerUnits = 1}, "Debug/lib4-cpp");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib3-cpp");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib2-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Touching lib3.cpp
    const path publicLib3DotCpp = testSourcePath / "lib3/private/lib3.cpp";
    touchFile(publicLib3DotCpp);
    BALANCES(Updates{.nodesFile = true}, "Debug/lib4-cpp");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib3-cpp");
    BALANCES(Updates{}, "Debug/lib2");
    BALANCES(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching public-lib4.hpp
    const path publicLib4DotHpp = testSourcePath / "lib4/public/public-lib4.hpp";
    touchFile(publicLib4DotHpp);
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 1, .linkTargetsNoDebug = 1, .nodesFile = true}, "Debug/lib4");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib3-cpp");
    BALANCES(Updates{.moduleFiles = 1, .linkTargetsNoDebug = 1}, "Debug/lib2");
    BALANCES(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching public-lib3.hpp
    const path publicLib3DotHpp = testSourcePath / "lib3/public/public-lib3.hpp";
    touchFile(publicLib3DotHpp);
    BALANCES(Updates{.headerUnits = 1, .nodesFile = true}, "Debug/lib4-cpp");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib3-cpp");
    BALANCES(Updates{.moduleFiles = 1, .linkTargetsNoDebug = 1}, "Debug/lib2");
    BALANCES(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching public-lib4.hpp
    touchFile(publicLib4DotHpp);
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 1, .linkTargetsNoDebug = 1, .nodesFile = true}, "Debug/lib4");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib3-cpp");
    BALANCES(Updates{.moduleFiles = 1, .linkTargetsNoDebug = 1}, "Debug/lib2");
    BALANCES(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Adding private compile-definition to lib3.
    copyFilePath(testSourcePath / "Version/hmakev3.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.headerUnits = 1}, "Debug/lib4-cpp");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib3-cpp");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib2-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Removing private compile-definition to lib3.
    copyFilePath(testSourcePath / "Version/hmakev1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.headerUnits = 1}, "Debug/lib4-cpp");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib3-cpp");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib2-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Just an extra re-configuration test.
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{}, "Debug/lib4-cpp");

    // Making public-lib4.hpp and private-lib4.hpp header-units. compile-definition removed as well.
    copyFilePath(testSourcePath / "Version/hmakev2.cpp", testSourcePath / "hmake.cpp");
    // private-lib4.hpp, public-lib4.hpp, public-lib3.hpp, lib3.cpp, lib4.cpp.
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.headerUnits = 3, .moduleFiles = 2}, "Debug/lib3-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib3");
    BALANCES(Updates{.moduleFiles = 1, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Making public-lib4.hpp and private-lib4.hpp header-files.
    copyFilePath(testSourcePath / "Version/hmakev1.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 2}, "Debug/lib3-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib4");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib3");
    BALANCES(Updates{.moduleFiles = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Making public-lib4.hpp and private-lib4.hpp header-units again. Should not be recompiled.
    copyFilePath(testSourcePath / "Version/hmakev2.cpp", testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 3}, "Debug/lib1-cpp");
    BALANCES(Updates{}, "Debug/app-cpp");
    BALANCES(Updates{}, "Debug/lib1");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib2");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib3");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib4");
    BALANCES(Updates{.linkTargetsDebug = 1}, "Debug/app");

    //  Touching public-lib4.hpp. lib3.cpp has a header-unit dep on public-lib3.hpp which has a header-dep on
    //  public-lib4.hpp, i.e. public-lib3.hpp will be recompiled and its dependent lib3.cpp will also be recompiled
    touchFile(testSourcePath / "lib4/public/public-lib4.hpp");

    // lib2.cpp, lib3.cpp, lib4.cpp, public-lib3.hpp, public-lib4.hpp.
    BALANCES(Updates{.moduleFiles = 4, .nodesFile = true}, "Debug/lib3-cpp");
    BALANCES(Updates{.moduleFiles = 1}, "Debug/lib1-cpp");
    BALANCES(Updates{.linkTargetsNoDebug = 1}, "Debug/lib2");
    BALANCES(Updates{.linkTargetsNoDebug = 2, .linkTargetsDebug = 1});
}

// Test for cycle in modules. In first case, we add an inverse module relationship between two modules. Then we fix
// build then we add cycle involving three modules.
TEST(StageTests, Test5)
{
    const path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test5");
    const path example8Path = path(SOURCE_DIRECTORY) / path("Examples/Example8");

    copyFilePath(testSourcePath / "Version/tenv0.cppm", example8Path / "Mod_Src/ten.cppm");
    copyFilePath(testSourcePath / "Version/fifteenv0.cppm", example8Path / "Mod_Src/fifteen.cppm");

    current_path(example8Path);

    ExamplesTestHelper::cleanBuild();
    ExamplesTestHelper::runAppWithExpectedOutput(current_path().string() + "/Release/app/" +
                                                     getActualNameFromTargetName(TargetType::EXECUTABLE, os, "app"),
                                                 "Hello World\n");

    // Clean build succeeds. two.cpp depends on ten.cppm. We edit ten.cppm to depend on two.cppm.
    copyFilePath(testSourcePath / "Version/tenv1.cppm", example8Path / "Mod_Src/ten.cppm");

    {
        string twoPath = (path(SOURCE_DIRECTORY) / path("Examples/Example8/Mod_Src/two.cppm")).string();
        string tenPath = (path(SOURCE_DIRECTORY) / path("Examples/Example8/Mod_Src/ten.cppm")).string();

        current_path(example8Path / "Build");
        RunCommand r;
        r.runProcess("hbuild");
        erase_if(*r.output, [](const char c) { return c == '\r'; });
        int exitStatus = r.exitStatus;
        string output = std::move(*r.output);
        ASSERT_EQ(exitStatus, EXIT_FAILURE);
        const string str1 =
            "error: Dependency graph contains a cycle.\nCycle: " + twoPath + " -> " + tenPath + " -> " + twoPath + "\n";
        const string str2 =
            "error: Dependency graph contains a cycle.\nCycle: " + tenPath + " -> " + twoPath + " -> " + tenPath + "\n";
        const string result = removeColorCodes(output);
        printMessage("comparing output\n");
        ASSERT_TRUE(result == str1 || result == str2) << "Actual output was: " << result;
    }

    // We correct the older cycle.
    copyFilePath(testSourcePath / "Version/tenv0.cppm", example8Path / "Mod_Src/ten.cppm");
    // build returns successfully but no file is built as content-hashing is the same.
    BALANCES(Updates{.nodesFile = true}, example8Path / "Build");

    // We add a bigger cycle this time
    // We modify fifteen.cppm to depend on seven.cppm. but sever.cppm already -> fourteen.cppm -> fifteen.cppm.
    copyFilePath(testSourcePath / "Version/fifteenv1.cppm", example8Path / "Mod_Src/fifteen.cppm");

    {
        string sevenPath = (path(SOURCE_DIRECTORY) / path("Examples/Example8/Mod_Src/seven.cppm")).string();
        string fourteenPath = (path(SOURCE_DIRECTORY) / path("Examples/Example8/Mod_Src/fourteen.cppm")).string();
        string fifteenPath = (path(SOURCE_DIRECTORY) / path("Examples/Example8/Mod_Src/fifteen.cppm")).string();

        current_path(example8Path / "Build");
        RunCommand r;
        r.runProcess("hbuild");
        erase_if(*r.output, [](const char c) { return c == '\r'; });
        int exitStatus = r.exitStatus;
        string output = std::move(*r.output);
        ASSERT_EQ(exitStatus, EXIT_FAILURE);
        const string str = "error: Dependency graph contains a cycle.\nCycle: " + sevenPath + " -> " + fourteenPath +
                           " -> " + fifteenPath + " -> " + sevenPath + "\n";
        const string result = removeColorCodes(output);
        ASSERT_EQ(result, str);
    }

    copyFilePath(testSourcePath / "Version/tenv0.cppm", example8Path / "Mod_Src/ten.cppm");
    copyFilePath(testSourcePath / "Version/fifteenv0.cppm", example8Path / "Mod_Src/fifteen.cppm");
}

// Tests a custom code generator (HeaderGenerator). This generator generates a header-file based on input file. This
// code-generator is built as part of the build. This ensures that a consuming module is built if any file of the
// code-generator is changed, or input-command of code-generator is changed or the input-file of the code-generator is
// changed.
TEST(StageTests, Test6)
{
    const path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/Stage/Test6");

    const path hmakeVersion0 = testSourcePath / "Version/hmakev0.cpp";
    const path hmakeVersion1 = testSourcePath / "Version/hmakev1.cpp";

    const path tool2Version0 = testSourcePath / "Version/tool2v0.cpp";
    const path tool2Version1 = testSourcePath / "Version/tool2v1.cpp";

    const path valueVersion0 = testSourcePath / "Version/valuev0.txt";
    const path valueVersion1 = testSourcePath / "Version/valuev1.txt";

    copyFilePath(hmakeVersion0, testSourcePath / "hmake.cpp");
    copyFilePath(tool2Version0, testSourcePath / "tool2.cpp");
    copyFilePath(valueVersion0, testSourcePath / "value.txt");

    current_path(testSourcePath);
    ExamplesTestHelper::cleanBuild();

    ExamplesTestHelper::runAppWithExpectedOutput(testSourcePath / "Build/Release/app/app", "20\n");

    BALANCES(Updates{});

    const path toolCppFilePath = testSourcePath / path("tool.cpp");
    touchFile(toolCppFilePath);

    // app-hu.ifc, tool.cpp, app.cpp --- tool, app -- output.h
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 2, .linkTargetsDebug = 2, .generatedHeaders = 1, .nodesFile = true});

    touchFile(toolCppFilePath);
    BALANCES(Updates{.moduleFiles = 1, .linkTargetsDebug = 1, .nodesFile = true}, "Release/tool");
    BALANCES(Updates{.generatedHeaders = 1, .nodesFile = true}, "Release/IncGen");
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 1, .linkTargetsDebug = 1});

    const path toolDepBuildDir = testSourcePath / "Build/Release/tooldep-cpp";

    // removing dependency build-directory contents. only hu should be built. as hu is not being consumed anywhere
    removeFilePath(toolDepBuildDir, true);
    BALANCES(Updates{.headerUnits = 1}, "Release/tooldep-cpp");
    BALANCES(Updates{});

    const path toolBuildDir = testSourcePath / "Build/Release/tool-cpp";

    // removing tool-cpp build-directory contents. header-gen should be built as-well as the tool would be updated.
    removeFilePath(toolBuildDir, true);
    BALANCES(Updates{.moduleFiles = 2, .linkTargetsDebug = 1}, "Release/tool");
    BALANCES(Updates{.generatedHeaders = 1, .nodesFile = true}, "Release/IncGen");
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 1, .linkTargetsDebug = 1});

    // tool2 includes the hu by tool2-dep
    copyFilePath(tool2Version1, testSourcePath / "tool2.cpp");
    BALANCES(Updates{.nodesFile = true}, "Release/tooldep-cpp");
    BALANCES(Updates{.moduleFiles = 1}, "Release/tool-cpp");
    BALANCES(Updates{.linkTargetsDebug = 1, .generatedHeaders = 1, .nodesFile = true}, "Release/IncGen");
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 1, .linkTargetsDebug = 1});

    const path toolDepHuDepHeader = testSourcePath / "tool-hu-header.hpp";
    touchFile(toolDepHuDepHeader);
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 1, .nodesFile = true}, "Release/tool-cpp");
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 1, .linkTargetsDebug = 2, .generatedHeaders = 1, .nodesFile = true});

    // We have tested the header-gen tool correctly generating the header-file. Now we would test the
    // dependency-specification. We add the app2.cpp dependency on header-gen and then remove it. In both cases it would
    // be rebuilt even though it never included the output.h header-file.

    copyFilePath(hmakeVersion1, testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{}, "Release/tool-cpp");
    BALANCES(Updates{.moduleFiles = 1, .linkTargetsDebug = 1});

    copyFilePath(hmakeVersion0, testSourcePath / "hmake.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    BALANCES(Updates{}, "Release/tool-cpp");
    BALANCES(Updates{.moduleFiles = 1, .linkTargetsDebug = 1});

    copyFilePath(valueVersion1, testSourcePath / "value.txt");
    BALANCES(Updates{.nodesFile = true}, "Release/tooldep-cpp");
    BALANCES(Updates{}, "Release/tool-cpp");
    BALANCES(Updates{.generatedHeaders = 1, .nodesFile = true}, "Release/IncGen");
    BALANCES(Updates{.headerUnits = 1, .moduleFiles = 1, .linkTargetsDebug = 1});

    ExamplesTestHelper::runAppWithExpectedOutput(testSourcePath / "Build/Release/app/app", "30\n");
}

// TODO
// Few features like PLOAT::outputName and PLOAT::directory aren't tested. atm.
// standard header-files caching, standard header-units caching and ignore-header-deps has not been tested as well.
// Testing could be further expanded as-well to test all the the error-messages.
