
#ifdef USE_HEADER_UNITS
import "BuildTools.hpp";
import "ToolsCache.hpp";
import "fmt/format.h";
import <filesystem>;
import <fstream>;
import set>;
#else
#include "BuildTools.hpp"
#include "ToolsCache.hpp"
#include "fmt/format.h"
#include <filesystem>
#include <fstream>
#include <set>
#endif

using std::string, std::vector, std::ifstream, std::ofstream, std::endl, std::filesystem::path,
    std::filesystem::current_path, std::filesystem::directory_iterator, std::filesystem::recursive_directory_iterator,
    std::to_string, std::runtime_error, std::filesystem::canonical, fmt::print, std::set;
using Json = nlohmann::json;

vector<string> headerFiles;
vector<string> ignoredFiles;
vector<path> dirs;
set<path> dirsSet;

bool recursive = false;

void writeHeaderFile(const path &dirPath, const vector<string> &extensions)
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
                headerFiles.emplace_back(p.path().string());
                headerFilesLocal.emplace_back(p.path().filename().string());
            }
            else
            {
                ignoredFiles.emplace_back(p.path().string());
            }
        }
        if (p.is_directory())
        {
            if (recursive && !dirsSet.contains(p.path()))
            {
                dirsSet.emplace(p.path());
                dirs.emplace_back(p.path());
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

int main(const int argc, char **argv)
{
    vector<string> arguments;
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
        path p = current_path();
        dirs.emplace_back(p);
        for (const auto &dir : recursive_directory_iterator(p))
        {
            if (dir.is_directory())
            {
                dirsSet.emplace(dir.path());
                dirs.emplace_back(dir.path());
            }
        }

        unsigned int i = 0;
        while (i < dirs.size())
        { // Iterates over dynamically growing vector
            writeHeaderFile(dirs[i], arguments);
            ++i;
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
