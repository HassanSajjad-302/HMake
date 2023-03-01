

#ifndef HMAKE_EXAMPLESTESTHELPER_HPP
#define HMAKE_EXAMPLESTESTHELPER_HPP

#include <string>
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

struct ExamplesTestHelper
{
    static void recreateBuildDirAndBuildHMakeProject(bool onlyPackageNoProject = false);
    static void runAppWithExpectedOutput(const string &appName, const string &expectedOutput);
    static void recreateBuildDir();
};

#endif // HMAKE_EXAMPLESTESTHELPER_HPP
