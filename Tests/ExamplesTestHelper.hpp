

#ifndef HMAKE_EXAMPLESTESTHELPER_HPP
#define HMAKE_EXAMPLESTESTHELPER_HPP

#include "BuildSystemFunctions.hpp"
#include "Features.hpp"
#include <string>
using std::string;

static string hbuildBuildStr = getActualNameFromTargetName(TargetType::EXECUTABLE, os, "hbuild");
static string hhelperStr = getActualNameFromTargetName(TargetType::EXECUTABLE, os, "hhelper");

struct ExamplesTestHelper
{
    static void recreateBuildDirAndBuildHMakeProject();
    static void runAppWithExpectedOutput(const string &appName, const string &expectedOutput);
    static void recreateBuildDirAndGethbuildOutput(string &output, int32_t exitStatus);
    static void runCommandAndGetOutput(const string &command, string &output);
    static void runCommandAndGetOutputInDirectory(const string &dir, const string &command, string &output);
    static void recreateBuildDir();
};

#endif // HMAKE_EXAMPLESTESTHELPER_HPP
