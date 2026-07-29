#include "BuildSystemFunctions.hpp"
#include "Features.hpp"
#include "TargetType.hpp"
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <string>

using std::filesystem::current_path, std::filesystem::directory_iterator, std::ifstream, std::filesystem::path,
    std::filesystem::exists, std::string, std::format;

int runCommand(const char *cmd)
{
#ifdef _WIN32
    return system(cmd);
#else
    const int status = system(cmd);
    if (status == -1)
    {
        printErrorMessage(FORMAT("Could not start the generated build executable.\nCommand: {}\nSystem error: {}", cmd,
                                 std::strerror(errno)));
        return -1; // system() itself failed
    }

    if (const int exitCode = WEXITSTATUS(status); exitCode == EXIT_SUCCESS)
    {
        return 0; // command succeeded
    }
    else
    {
        return exitCode; // propagate non-zero exit code
    }
#endif
}

int main(const int argc, char **argv)
{
    constructGlobals();
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
        return runCommand(str.c_str());
    }
    printErrorMessage(FORMAT("Could not find the generated build executable in the current directory or any parent.\n"
                             "Executable: {}\nCurrent directory: {}\n"
                             "Hint: run hhelper twice from the project's build directory first.",
                             buildExeName, current_path().string()));
    exit(EXIT_FAILURE);
}
