

#ifndef HMAKE_EXAMPLESTESTHELPER_HPP
#define HMAKE_EXAMPLESTESTHELPER_HPP

#include "Features.hpp"
#include <string>
using std::string;

static string hbuildBuildStr = getActualNameFromTargetName(TargetType::EXECUTABLE, os, "hbuild");
static string hhelperStr = getActualNameFromTargetName(TargetType::EXECUTABLE, os, "hhelper");

struct ExamplesTestHelper
{
    static void recreateBuildDirAndBuildHMakeProject();
    static void runAppWithExpectedOutput(const string &appName, const string &expectedOutput);
    static void recreateBuildDir();
};

#endif // HMAKE_EXAMPLESTESTHELPER_HPP
