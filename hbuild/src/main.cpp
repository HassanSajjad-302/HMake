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
#include "zDLLLoader.hpp"
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
        path configureSharedLibPath;
        bool configureSharedLibExists = false;
        string configureName = getActualNameFromTargetName(TargetType::LIBRARY_SHARED, os, "configure");
        for (path p = current_path(); p.root_path() != p; p = (p / "..").lexically_normal())
        {
            configureSharedLibPath = p / configureName;
            if (exists(configureSharedLibPath))
            {
                configureSharedLibExists = true;
                break;
            }
        }
        if (configureSharedLibExists)
        {
            DLLLoader loader(configureSharedLibPath.string().c_str());
            typedef int (*Func2)(BSMode bsMode);
            const auto func2 = loader.getSymbol<Func2>("func2");
            if (!func2)
            {
                printErrorMessage("Symbol func2 could not be loaded from configure dynamic library\n");
                exit(EXIT_FAILURE);
            }
            return func2(BSMode::BUILD);
        }
        printErrorMessage(
            fmt::format("{} File could not be found in current directory and directories above\n", configureName));
        exit(EXIT_FAILURE);
    }
}
