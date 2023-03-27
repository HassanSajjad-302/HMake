#ifdef USE_HEADER_UNITS
#include "DLLLoader.hpp"
#include "fmt/format.h"
#include <filesystem>
#include <fstream>
#include <string>
#else
#include "BuildSystemFunctions.hpp"
#include "DLLLoader.hpp"
#include "fmt/format.h"
#include <filesystem>
#include <fstream>
#include <string>
#endif

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
            auto func2 = loader.getSymbol<Func2>("func2");
            if (!func2)
            {
                print(stderr, "Symbol func2 could not be loaded from configure dynamic library\n");
                exit(EXIT_FAILURE);
            }
            int exitStatus = func2(BSMode::BUILD);
            if (exitStatus != EXIT_SUCCESS)
            {
                auto errorMessageStrPtrLocal = loader.getSymbol<const char **>("errorMessageStrPtr");
                if (!errorMessageStrPtrLocal)
                {
                    print(stderr,
                          "Symbol errorMessageStrPtrLocal could not be loaded from configure dynamic library\n");
                    exit(EXIT_FAILURE);
                }
                print(stderr, "{}\n", *errorMessageStrPtrLocal);
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            print(stderr, "{} File could not be found in current directory and directories above\n", configureName);
            exit(EXIT_FAILURE);
        }
    }
}
