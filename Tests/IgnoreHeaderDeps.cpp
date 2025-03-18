

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
#include <string>



//This should test whether InclNode::ignoreHeaderDeps works or not for the include dirs.
// What does this mean in the context of header-units and whether it works in that context as-well.

TEST(IgnoreHeaderDeps, Test1)
{ /*
     const path testSourcePath = path(SOURCE_DIRECTORY) / path("Tests/IgnoreHeaderDeps");
     current_path(testSourcePath);*/
    // setupTest3Default();

    /*
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
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 1, .cppTargets = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching public-lib3.hpp
    const path publicLib3DotHpp = testSourcePath / "lib3/public/public-lib3.hpp";
    touchFile(publicLib3DotHpp);
    executeSnapshotBalances(Updates{.smruleFiles = 1, .cppTargets = 1}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 1, .cppTargets = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 1}, "Debug/lib2");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Touching public-lib4.hpp
    const path publicLib4DotHpp = testSourcePath / "lib4/public/public-lib4.hpp";
    touchFile(publicLib4DotHpp);
    executeSnapshotBalances(Updates{.smruleFiles = 1, .cppTargets = 1}, "Debug/lib1-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1}, "Debug/lib2-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 1, .cppTargets = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1}, "Debug/lib2");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Making lib4.cpp and lib2.cpp both module-files.
    // 4 smrules files lib4.cpp lib3.cpp public-lib4.hpp privte-lib4.hpp will be updated
    copyFilePath(testSourcePath / "Version/3/hmake.cpp", testSourcePath / "hmake.cpp");
    copyFilePath(testSourcePath / "Version/3/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    copyFilePath(testSourcePath / "Version/3/lib2.cpp", testSourcePath / "lib2/private/lib2.cpp");
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.smruleFiles = 4, .cppTargets = 2}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 2, .cppTargets = 1}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 2, .linkTargetsDebug = 1});

    // Removing both header-units from lib4.cpp
    copyFilePath(testSourcePath / "Version/4/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    executeSnapshotBalances(Updates{.smruleFiles = 1, .cppTargets = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1}, "Debug/lib4-cpp");
    executeSnapshotBalances(Updates{.linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Adding both header-units back in lib4.cpp
    copyFilePath(testSourcePath / "Version/3/lib4.cpp", testSourcePath / "lib4/private/lib4.cpp");
    executeSnapshotBalances(Updates{.smruleFiles = 1, .cppTargets = 1}, "Debug/lib3-cpp");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1}, "Debug/lib4-cpp");
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
    executeSnapshotBalances(Updates{.sourceFiles = 1, .cppTargets = 1}, "Debug/lib4-cpp");
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
    const path cacheFile = testSourcePath / "Build/cache.json";
    Json cacheJson;
    ifstream(cacheFile) >> cacheJson;
    // Moving from module to source, lib4.cpp will be recompiled because lib4.cpp.o was not compiled with latest
    // changes. Other file is lib2.cpp.o which is being recompiled because public-lib4.hpp is being recompiled because
    // of change of compile-command due to removal of compile-definition.
    cacheJson["cache-variables"]["use-module"] = false;
    ofstream(cacheFile) << cacheJson.dump(4);
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.smruleFiles = 1,
                                    .sourceFiles = 2,
                                    .moduleFiles = 1,
                                    .cppTargets = 1,
                                    .linkTargetsNoDebug = 2,
                                    .linkTargetsDebug = 1});

    cacheJson["cache-variables"]["use-module"] = true;
    ofstream(cacheFile) << cacheJson.dump(4);
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.smruleFiles = 1, .cppTargets = 1}, "Debug/lib3");
    executeSnapshotBalances(Updates{.sourceFiles = 1, .moduleFiles = 1, .linkTargetsNoDebug = 1}, "Debug/lib2");

    // linker prints "fatal error" but still builds the lib fine and snapshot balances. Just to be safe the module
    // object-files are now specified in order. This shouldn't be needed as object-files ordering specification should
    // not matter to linker. Changes in this commit will probably be reverted.
    executeSnapshotBalances(Updates{.sourceFiles = 1, .linkTargetsNoDebug = 1, .linkTargetsDebug = 1});

    // Moving back to source from module. lib4.cpp.o should not be rebuilt because lib4.cpp with the same
    // compile-command is already in the cache but relinking should happen because previously it were source-file
    // object-files, now it is module-file object-files.
    cacheJson["cache-variables"]["use-module"] = false;
    ofstream(cacheFile) << cacheJson.dump(4);
    ASSERT_EQ(system(hhelperStr.c_str()), 0) << hhelperStr + " command failed.";
    executeSnapshotBalances(Updates{.smruleFiles = 1,
                                    .sourceFiles = 1,
                                    .moduleFiles = 1,
                                    .cppTargets = 1,
                                    .linkTargetsNoDebug = 2,
                                    .linkTargetsDebug = 1});*/
}
