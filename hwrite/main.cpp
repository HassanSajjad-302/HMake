
#ifdef USE_HEADER_UNITS
import "BuildTools.hpp";
import "fmt/format.h";
import "JConsts.hpp";
import "ToolsCache.hpp";
import "Utilities.hpp";
import "zDLLLoader.hpp";
import <filesystem>;
import <fstream>;
#else
#include "BuildTools.hpp"
#include "JConsts.hpp"
#include "ToolsCache.hpp"
#include "Utilities.hpp"
#include "fmt/format.h"
#include "zDLLLoader.hpp"
#include <filesystem>
#include <fstream>
#endif

using std::string, std::vector, std::ifstream, std::ofstream, std::endl, std::filesystem::path,
    std::filesystem::current_path, std::filesystem::directory_iterator, std::filesystem::recursive_directory_iterator,
    std::to_string, std::runtime_error, std::filesystem::canonical, fmt::print;
using Json = nlohmann::json;

vector<string> headerFiles;
vector<string> ignoredFiles;
void writeHeaderFile(const path &dirPath, vector<string> &extensions)
{
    vector<string> headerFilesLocal;
    for (const auto &p : directory_iterator(dirPath))
    {
        if (p.is_regular_file())
        {
            bool headerFile = false;
            for (const string &str : extensions)
            {
                string ext = p.path().extension().string();
                if (p.path().extension() == str)
                {
                    headerFile = true;
                    break;
                }
            }
            if (headerFile)
            {
                headerFiles.emplace_back(p.path().generic_string());
                headerFilesLocal.emplace_back(p.path().filename().generic_string());
            }
            else
            {
                ignoredFiles.emplace_back(p.path().generic_string());
            }
        }
    }

    if (!headerFilesLocal.empty())
    {
        Json j;
        j["Version"] = "1.0";
        j["BuildAsHeaderUnits"] = headerFilesLocal;

        ofstream(dirPath / "header-units.json") << j.dump(4);
    }
}

int main(int argc, char **argv)
{
    vector<string> arguments;
    bool recursive = false;
    for (size_t i = 1; i < argc; ++i)
    {
        if (argv[i] == string("-r"))
        {
            recursive = true;
        }
        else
        {
            arguments.emplace_back(argv[i]);
            print("argument {}\n", argv[i]);
        }
    }

    if (recursive)
    {
        writeHeaderFile(current_path(), arguments);
        for (const auto &p : recursive_directory_iterator(current_path()))
        {
            if (p.is_directory())
            {
                writeHeaderFile(p.path(), arguments);
            }
        }
    }
    else
    {
        writeHeaderFile(current_path(), arguments);
    }

    print("{}\n", "Header Files");
    for (const string &f : headerFiles)
    {
        print("{}\n", f);
    }
    print("{}\n", "Ignored Files");
    for (const string &str : ignoredFiles)
    {
        print("{}\n", str);
    }
}
