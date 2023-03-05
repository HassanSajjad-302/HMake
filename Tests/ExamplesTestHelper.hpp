

#ifndef HMAKE_EXAMPLESTESTHELPER_HPP
#define HMAKE_EXAMPLESTESTHELPER_HPP

#include <string>
using std::string;

static string hbuildBuildStr = "hbuild";
static string hhelperStr = "hhelper";

struct ExamplesTestHelper
{
    static void recreateBuildDirAndBuildHMakeProject();
    static void runAppWithExpectedOutput(const string &appName, const string &expectedOutput);
    static void recreateBuildDir();
};

#endif // HMAKE_EXAMPLESTESTHELPER_HPP
