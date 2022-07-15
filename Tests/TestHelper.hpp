

#ifndef HMAKE_TESTHELPER_HPP
#define HMAKE_TESTHELPER_HPP

#include "string"
using std::string;

inline string getExeName(const string &exeName)
{
#ifdef _WIN32
    return exeName + ".exe";
#else
    return exeName;
#endif
}

inline string getSlashedExeName(const string &exeName)
{
#ifdef _WIN32
    return exeName;
#else
    return "./" + exeName;
#endif
}

inline string getArchiveName(const string &libName)
{
#ifdef _WIN32
    return "lib" + libName + ".a";
#else
    return libName;
#endif
}

struct TestHelper
{
    static void recreateBuildDirAndBuildHMakeProject(bool onlyPackageNoProject = false);
    static void runAppWithExpectedOutput(const string &appName, const string &expectedOutput);
    static void recreateBuildDir();
};

#endif // HMAKE_TESTHELPER_HPP
