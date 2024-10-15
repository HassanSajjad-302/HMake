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
    if (argc == 2)
    {
        const std::string filePath = argv[1];
        path buildFilePath = filePath;
        if (buildFilePath.is_relative())
        {
            buildFilePath = (current_path() / buildFilePath).lexically_normal();
        }
        else
        {
            buildFilePath = buildFilePath.lexically_normal();
        }
        if (!is_regular_file(buildFilePath))
        {
            printMessage(fmt::format("{} is not regular file.\n", buildFilePath.string()));
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        path configureExecutablePath;
        bool configuredExecutableExists = false;
        string configureName = getActualNameFromTargetName(TargetType::EXECUTABLE, os, "configure");
        for (path p = current_path(); p.root_path() != p; p = (p / "..").lexically_normal())
        {
            configureExecutablePath = p / configureName;
            if (exists(configureExecutablePath))
            {
                configuredExecutableExists = true;
                break;
            }
        }
        if (configuredExecutableExists)
        {
            const string str = configureExecutablePath.string() + " --build";
            return system(str.c_str());
        }
        printErrorMessage(
            fmt::format("{} File could not be found in current directory and directories above\n", configureName));
        exit(EXIT_FAILURE);
    }
}
