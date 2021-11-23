

#ifndef HMAKE_TESTHELPER_HPP
#define HMAKE_TESTHELPER_HPP

#include "string"
using std::string;

struct TestHelper
{

    static string runHMakeProject();
    static void recreateSourceAndBuildDir();
};

#endif // HMAKE_TESTHELPER_HPP
