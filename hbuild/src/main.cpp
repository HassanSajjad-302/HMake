#ifdef USE_HEADER_UNITS
import "BuildSystemFunctions.hpp";
import "Features.hpp";
#include "fmt/format.h"
import "TargetType.hpp";
import "zDLLLoader.hpp";
import <filesystem>;
import <string>;
#else
#include "BuildSystemFunctions.hpp"
#include "Features.hpp"
#include "TargetType.hpp"
#include "fmt/format.h"
#include <filesystem>
#include <string>
#endif

using std::filesystem::current_path, std::filesystem::directory_iterator, std::ifstream, std::filesystem::path,
    std::filesystem::exists, std::string, fmt::format;
int main(const int argc, char **argv)
{
    path buildExePath;
    bool configuredExecutableExists = false;
    string buildExeName = getActualNameFromTargetName(TargetType::EXECUTABLE, os, "build");
    for (path p = current_path(); p.root_path() != p; p = (p / "..").lexically_normal())
    {
        buildExePath = p / buildExeName;
        if (exists(buildExePath))
        {
            configuredExecutableExists = true;
            break;
        }
    }
    if (configuredExecutableExists)
    {
        string str = buildExePath.string() + ' ';
        for (uint64_t i = 1; i < argc; ++i)
        {
            str += argv[i];
            str += ' ';
        }
        return system(str.c_str());
    }
    printErrorMessage(FORMAT("{} File could not be found in current dir and dirs above\n", buildExeName));
    exit(EXIT_FAILURE);
}
