

#ifndef HMAKE_TESTHELPER_HPP
#define HMAKE_TESTHELPER_HPP

#include "string"
using std::string;

struct TestHelper
{

    static void runHMakeProjectWithExpectedOutput(const string &expectedOutput);
    static void recreateSourceAndBuildDir();
};

#endif // HMAKE_TESTHELPER_HPP
