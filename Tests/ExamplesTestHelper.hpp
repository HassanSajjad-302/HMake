

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
    static void recreateBuildDirAndGethbuildOutput(pstring &output, int32_t exitStatus);
    static void runCommandAndGetOutput(const pstring &command, pstring &output);
    static void runCommandAndGetOutputInDirectory(const pstring &directory, const pstring &command, pstring &output);
    static void recreateBuildDir();
};

#endif // HMAKE_EXAMPLESTESTHELPER_HPP
