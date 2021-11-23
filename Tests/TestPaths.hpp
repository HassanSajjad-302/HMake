

#ifndef HMAKE_TESTPATHS_HPP
#define HMAKE_TESTPATHS_HPP

#include "filesystem"
using std::string, std::ofstream, std::filesystem::create_directory, std::filesystem::path;

struct TestPaths
{

    static string compileAndRunHMakeProject(const string &hmakeFile, const string &mainSourceFile);
};

#endif // HMAKE_TESTPATHS_HPP
