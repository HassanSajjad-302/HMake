#include "Configure.hpp"

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>

bool replace(std::string &str, const std::string &from, const std::string &to)
{
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
    {
        printErrorMessage("Replace Failed\n");
    }
    str.replace(start_pos, from.length(), to);
    return true;
}


bool replaceNoError(std::string &str, const std::string &from, const std::string &to)
{
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
    {
        return false;
    }
    str.replace(start_pos, from.length(), to);
    return true;
}

int main()
{
    std::map<std::string, std::vector<std::string>> deps;
    std::string depsTxt = fileToString("deps.txt");
    vector<string_view> lines = split(depsTxt, '\n');

    for (uint32_t i = 0; i < lines.size(); ++i)
    {
        string line(lines[i]);

        // Skip empty lines
        if (line.empty())
            continue;

        // First line is the library name
        std::string libraryName = line;

        // Read the dependencies line
        ++i;
        line = lines[i];

        std::vector<std::string> dependencies;

        // Parse dependencies by splitting on "LLVM" prefix
        size_t pos = 0;
        while ((pos = line.find("LLVM", pos)) != std::string::npos)
        {
            // Find the next occurrence of "LLVM"
            size_t nextPos = line.find("LLVM", pos + 4);

            std::string dep;
            if (nextPos != std::string::npos)
            {
                dep = line.substr(pos, nextPos - pos);
            }
            else
            {
                // Last dependency in the line
                dep = line.substr(pos);
            }

            if (!dep.empty())
            {
                dependencies.emplace_back(dep);
            }

            pos += 4; // Move past "LLVM"
        }

        deps[libraryName] = dependencies;
    }

    // Print the parsed result
    for (const auto &[lib, libDeps] : deps)
    {
        std::cout << lib << " depends on: ";
        for (const auto &dep : libDeps)
        {
            std::cout << dep << " ";
        }
        std::cout << std::endl;
    }

    std::string hmakeFile = fileToString("hmake.cpp");

    for (const auto &[lib, libDeps] : deps)
    {
        string depsText;
        for (const auto &dep : libDeps)
        {
            string dep2 = dep;
            replace(dep2, "LLVM", "llvm");
            depsText +=  dep2 + ',';
        }
        depsText.pop_back();
        string from = "config.getCppStaticDSC(\"" + lib + "\")";
        string to = "config.getCppStaticDSC(\"" + lib + "\").publicDeps(";
        to += depsText + ")";

        if (!replaceNoError(hmakeFile, from, to))
        {
            printMessage(FORMAT("Library {} not found.\n", lib));
        }
    }

    std::cout << "\n\n\n\n\n\n\n\n";

    std::cout << hmakeFile << "\n\n";
    std::ofstream("hmake.cpp") << hmakeFile;

    return 0;
}