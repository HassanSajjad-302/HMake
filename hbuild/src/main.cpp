
#include "BuildSystemFunctions.hpp"
#include "fmt/format.h"
#include <filesystem>
#include <fstream>
#include <string>

using std::filesystem::current_path, std::filesystem::directory_iterator, std::ifstream, fmt::print,
    std::filesystem::path, std::filesystem::exists, std::string;
int main(int argc, char **argv)
{
    if (argc == 2)
    {
        std::string filePath = argv[1];
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
            fmt::print("{} is not regular file.\n", buildFilePath.string());
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        path configureFilePath;
        bool configureAppExists = false;
        string configureName = getActualNameFromTargetName(TargetType::EXECUTABLE, os, "configure");
        for (path p = current_path(); p.root_path() != p; p = (p / "..").lexically_normal())
        {
            configureFilePath = p / configureName;
            if (exists(configureFilePath))
            {
                configureAppExists = true;
                break;
            }
        }
        if (configureAppExists)
        {
            string str = configureFilePath.string() + " --build";
            return system(str.c_str());
        }
        else
        {
            print(stderr, "{} File could not be found in current directory and directories above\n", configureName);
            exit(EXIT_FAILURE);
        }
    }
}
